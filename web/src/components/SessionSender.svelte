<script>
    import { t, subscribe } from '$lib/i18n.js';
    import { encryptChunk, keyToBase64url } from '$lib/crypto.js';
    import { buildAnchor } from '$lib/url.js';
    import { sendAction, sendBinary, on, off, waitForConnection, isConnected } from '$lib/ws.js';
    import MemberList from './MemberList.svelte';
    import ProgressPanel from './ProgressPanel.svelte';
    import SessionTimer from './SessionTimer.svelte';
    import QRModal from './QRModal.svelte';

    let { sessionData, encryptionKey, initialFile = null, myName = '', myId = '', oncomplete } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let file = $state(null);
    let uploading = $state(false);
    let uploadFinished = $state(false);
    let progress = $state(0);
    let bytesTransferred = $state(0);
    let totalBytes = $state(0);
    let bufferUsed = $state(
        Array.isArray(sessionData?.state?.chunks) ? sessionData.state.chunks.length : 0
    );
    let chunksSent = $state(0);
    let linkCopied = $state(false);
    let showQR = $state(false);
    let sessionExpirationIn = sessionData?.state?.expiration_in || 0;
    let errorMsg = $state('');

    let senderInfo = $state(sessionData?.members?.sender || null);

    // Update own name in member list when changed via NameBadge
    $effect(() => {
        if (myName && senderInfo && senderInfo.name !== myName) {
            senderInfo = { ...senderInfo, name: myName };
        }
    });
    let receivers = $state(sessionData?.members?.receivers || []);
    let fileInfo = $state(null);

    // Reserve 40 bytes for encryption overhead (24 nonce + 16 Poly1305 tag)
    const CRYPTO_OVERHEAD = 40;
    let maxChunkSize = (sessionData?.limits?.max_chunk_size || 65536) - CRYPTO_OVERHEAD;
    let bufferMax = sessionData?.limits?.max_chunk_queue || 10;

    let shareLink = $derived(
        sessionData?.session_id
            ? buildAnchor(sessionData.session_id, 'xchacha20-poly1305', keyToBase64url(encryptionKey))
            : ''
    );

    // Flow control: wait for server to confirm each chunk accepted
    let chunkAcceptedResolve = null;
    let chunkAllowedResolve = null;
    let canSendChunk = $state(true);

    function onNewReceiver(msg) {
        const r = msg?.data ?? msg;
        receivers = [...receivers, {
            name: r.name,
            id: r.id,
            is_online: true,
            current_chunk: undefined,
        }];
    }

    function onReceiverRemoved(msg) {
        const id = msg.data?.id ?? msg.id;
        receivers = receivers.filter(r => r.id !== id);
    }

    function onNameChanged(msg) {
        const d = msg.data || msg;
        receivers = receivers.map(r =>
            r.id === d.id ? { ...r, name: d.name, _flash: Date.now() } : r
        );
    }

    function onChunkDownload(msg) {
        const d = msg.data || msg;
        receivers = receivers.map(r => {
            if (r.id === d.id) {
                return { ...r, current_chunk: d.index };
            }
            return r;
        });
    }

    function onNewChunkAllowed(msg) {
        const d = msg.data || msg;
        canSendChunk = d.status;
        if (d.status === true && chunkAllowedResolve) {
            const resolve = chunkAllowedResolve;
            chunkAllowedResolve = null;
            resolve();
        }
    }

    function onNewChunkEvent(msg) {
        // Server confirmed a chunk was added to buffer
        bufferUsed++;
        if (chunkAcceptedResolve) {
            const resolve = chunkAcceptedResolve;
            chunkAcceptedResolve = null;
            resolve();
        }
    }

    function onChunkRemoved(msg) {
        const d = msg.data || msg;
        const removed = Array.isArray(d.id) ? d.id.length : 1;
        bufferUsed = Math.max(0, bufferUsed - removed);
    }

    function onBytesCount(msg) {
        const d = msg.data || msg;
        if (d.direction === 'from_sender') {
            // Server reports encrypted bytes; cap at file size for display
            bytesTransferred = Math.min(d.value, totalBytes);
            if (totalBytes > 0) progress = Math.min(1, bytesTransferred / totalBytes);
        }
    }

    let completeHandled = false;

    function onComplete(msg) {
        if (completeHandled) return;
        completeHandled = true;
        const d = msg.data || msg;
        oncomplete?.({ status: d.status || 'ok' });
    }

    function onWsClose() {
        if (completeHandled) return;
        setTimeout(() => {
            if (completeHandled) return;
            completeHandled = true;
            oncomplete?.({ status: uploadFinished ? 'ok' : 'error' });
        }, 500);
    }

    function onOnline(msg) {
        const d = msg.data || msg;
        receivers = receivers.map(r =>
            r.id === d.id ? { ...r, is_online: d.status, _flash: Date.now() } : r
        );
    }

    // WS event handlers — no reactive dependencies to prevent re-registration
    $effect(() => {
        on('new_receiver', onNewReceiver);
        on('receiver_removed', onReceiverRemoved);
        on('name_changed', onNameChanged);
        on('chunk_download', onChunkDownload);
        on('new_chunk_allowed', onNewChunkAllowed);
        on('new_chunk', onNewChunkEvent);
        on('chunk_removed', onChunkRemoved);
        on('chunks_unfrozen', onChunksUnfrozen);
        on('bytes_count', onBytesCount);
        on('complete', onComplete);
        on('online', onOnline);
        on('close', onWsClose);

        return () => {
            off('new_receiver', onNewReceiver);
            off('receiver_removed', onReceiverRemoved);
            off('name_changed', onNameChanged);
            off('chunk_download', onChunkDownload);
            off('new_chunk_allowed', onNewChunkAllowed);
            off('new_chunk', onNewChunkEvent);
            off('chunk_removed', onChunkRemoved);
            off('chunks_unfrozen', onChunksUnfrozen);
            off('bytes_count', onBytesCount);
            off('complete', onComplete);
            off('online', onOnline);
            off('close', onWsClose);
        };
    });

    // Auto-start upload if file was pre-selected (runs once)
    // Deferred past paint cycle to avoid Firefox "interrupted while loading" issue
    let autoStarted = false;
    $effect(() => {
        if (initialFile && !autoStarted) {
            autoStarted = true;
            requestAnimationFrame(() => {
                setTimeout(() => handleFileSelect(initialFile), 0);
            });
        }
    });

    function handleFileSelect(selectedFile) {
        file = selectedFile;
        fileInfo = { name: selectedFile.name, size: selectedFile.size };
        totalBytes = selectedFile.size;
        sendAction('set_file_info', { name: selectedFile.name, size: selectedFile.size });
        startUpload();
    }

    async function waitForChunkAccepted() {
        await new Promise((resolve) => {
            chunkAcceptedResolve = resolve;
        });
    }

    async function waitForBufferSpace() {
        if (canSendChunk) return;
        await new Promise((resolve) => {
            chunkAllowedResolve = resolve;
        });
    }

    async function startUpload() {
        if (!file || uploading) return;
        uploading = true;
        errorMsg = '';
        bytesTransferred = 0;
        chunksSent = 0;

        try {
            const totalChunks = Math.ceil(file.size / maxChunkSize);
            console.log('[Sender] upload start:', { fileSize: file.size, maxChunkSize, totalChunks });
            for (let i = 0; i < totalChunks; i++) {

                const start = i * maxChunkSize;
                const end = Math.min(start + maxChunkSize, file.size);
                const slice = file.slice(start, end);
                const rawData = new Uint8Array(await slice.arrayBuffer());

                const encrypted = encryptChunk(rawData, encryptionKey);
                if (!sendBinary(encrypted)) {
                    // WS disconnected — wait for reconnect then retry this chunk
                    await waitForConnection();
                    i--; // retry this chunk
                    continue;
                }

                // Wait for server to confirm chunk was accepted (new_chunk event)
                // After this, bufferUsed is already incremented by onNewChunkEvent
                await waitForChunkAccepted();
                chunksSent++;

                // If buffer is full, wait for server to free space
                // No need for setTimeout — bufferUsed is updated synchronously
                // in the same handler that resolved the promise
                if (bufferUsed >= bufferMax) {
                    canSendChunk = false;
                    await waitForBufferSpace();
                }
            }

            sendAction('upload_finished', {});
            uploadFinished = true;
        } catch (err) {
            errorMsg = err.message || String(err);
        }
    }

    function handleKick(detail) {
        sendAction('kick_receiver', { id: detail.id });
    }

    let frozen = $state(sessionData?.state?.initial_freeze ?? true);
    let freezeMaxSec = sessionData?.limits?.max_initial_freeze || 120;
    let freezeRemaining = $state(freezeMaxSec);
    let freezeProgress = $derived(frozen ? freezeRemaining / freezeMaxSec : 0);

    // Countdown timer for freeze
    $effect(() => {
        if (!frozen) return;
        const interval = setInterval(() => {
            freezeRemaining = Math.max(0, freezeRemaining - 1);
            if (freezeRemaining <= 0) clearInterval(interval);
        }, 1000);
        return () => clearInterval(interval);
    });

    function handleDropFreeze() {
        sendAction('drop_freeze', {});
    }

    function onChunksUnfrozen() {
        frozen = false;
    }

    function handleTerminate() {
        const ok = sendAction('terminate_session', {});
        console.log('[Sender] terminate_session sent:', ok);
    }

    async function copyLink() {
        try {
            await navigator.clipboard.writeText(shareLink);
            linkCopied = true;
            setTimeout(() => { linkCopied = false; }, 2000);
        } catch {
            // fallback
            const ta = document.createElement('textarea');
            ta.value = shareLink;
            document.body.appendChild(ta);
            ta.select();
            document.execCommand('copy');
            document.body.removeChild(ta);
            linkCopied = true;
            setTimeout(() => { linkCopied = false; }, 2000);
        }
    }
</script>

<div class="session">
    <div class="sidebar">
        <MemberList
            sender={senderInfo}
            {receivers}
            isSender={true}
            onkick={handleKick}
        />
        {#if frozen && receivers.length > 0}
            <button class="freeze-btn pulse" onclick={handleDropFreeze}>
                <span class="freeze-bar" style="width: {Math.round(freezeProgress * 100)}%"></span>
                <span class="freeze-label">{tt('startTransfer')} ({freezeRemaining}{tt('seconds')})</span>
            </button>
        {/if}
        {#if frozen && receivers.length === 0}
            <div class="freeze-waiting">
                {tt('freezeExpires')} ({freezeRemaining}{tt('seconds')})
            </div>
        {:else}
            <SessionTimer expirationIn={sessionExpirationIn} />
        {/if}
        <button class="terminate-btn" onclick={handleTerminate}>
            {tt('terminateSession')}
        </button>
    </div>

    <div class="main">
        <div class="share-section">
            <label>{tt('shareLink')}</label>
            <div class="link-row">
                <input type="text" readonly value={shareLink} />
                <button onclick={copyLink}>
                    {linkCopied ? tt('copied') : tt('copyLink')}
                </button>
                <button class="qr-btn" onclick={() => showQR = true}>{tt('showQR')}</button>
            </div>
        </div>

        <ProgressPanel
            {fileInfo}
            {progress}
            {bytesTransferred}
            {totalBytes}
            isSender={true}
            {bufferUsed}
            {bufferMax}
            {uploadFinished}
            chunks={[]}
            onfileselect={handleFileSelect}
        />

        {#if errorMsg}
            <div class="error">{errorMsg}</div>
        {/if}
    </div>
</div>

{#if showQR}
    <QRModal url={shareLink} onclose={() => showQR = false} />
{/if}

<style>
    .session {
        display: grid;
        grid-template-columns: 280px 1fr;
        gap: 1rem;
        padding: 1rem;
        font-family: system-ui, -apple-system, sans-serif;
        max-width: 900px;
        margin: 0 auto;
    }

    .sidebar {
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
    }

    .freeze-btn {
        padding: 0.5rem;
        background: #0f3460;
        color: #eee;
        border: 1px solid #1a4a80;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85rem;
        font-family: inherit;
        position: relative;
        overflow: hidden;
    }

    .freeze-btn:hover {
        background: #1a4a80;
    }

    .freeze-bar {
        position: absolute;
        top: 0;
        left: 0;
        height: 100%;
        background: rgba(233, 69, 96, 0.25);
        transition: width 1s linear;
        pointer-events: none;
    }

    .freeze-label {
        position: relative;
        z-index: 1;
    }

    .freeze-waiting {
        color: #999;
        font-size: 0.75rem;
        text-align: center;
        padding: 0.4rem 0;
    }

    .freeze-btn.pulse {
        animation: pulse-glow 2s ease-in-out 3;
    }

    @keyframes pulse-glow {
        0%, 100% { box-shadow: 0 0 0 0 rgba(15, 52, 96, 0); }
        50% { box-shadow: 0 0 8px 2px rgba(15, 52, 96, 0.6); border-color: #e94560; }
    }

    .terminate-btn {
        padding: 0.5rem;
        background: rgba(233, 69, 96, 0.15);
        color: #e94560;
        border: 1px solid #e94560;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85rem;
        font-family: inherit;
    }

    .terminate-btn:hover {
        background: rgba(233, 69, 96, 0.3);
    }

    .main {
        display: flex;
        flex-direction: column;
        gap: 1rem;
    }

    .share-section {
        background: #16213e;
        border-radius: 8px;
        padding: 1rem;
    }

    .share-section label {
        color: #999;
        font-size: 0.85rem;
        display: block;
        margin-bottom: 0.5rem;
    }

    .link-row {
        display: flex;
        gap: 0.5rem;
    }

    .link-row input {
        flex: 1;
        padding: 0.5rem;
        background: #1a1a2e;
        border: 1px solid #0f3460;
        border-radius: 4px;
        color: #eee;
        font-size: 0.8rem;
        font-family: monospace;
        outline: none;
    }

    .link-row button {
        padding: 0.5rem 0.75rem;
        background: #0f3460;
        color: #eee;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85rem;
        white-space: nowrap;
        font-family: inherit;
    }

    .link-row button:hover {
        background: #1a4a80;
    }

    .error {
        color: #e94560;
        font-size: 0.85rem;
        padding: 0.5rem;
        background: rgba(233, 69, 96, 0.1);
        border-radius: 4px;
    }

    @media (max-width: 640px) {
        .session {
            grid-template-columns: 1fr;
        }
    }
</style>
