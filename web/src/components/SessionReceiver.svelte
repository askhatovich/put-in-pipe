<script>
    import { t, subscribe } from '$lib/i18n.js';
    import { decryptChunk } from '$lib/crypto.js';
    import { downloadChunk, leave } from '$lib/api.js';
    import { sendAction, on, off, waitForConnection, isConnected, disconnect } from '$lib/ws.js';
    import MemberList from './MemberList.svelte';
    import SessionTimer from './SessionTimer.svelte';
    import ProgressPanel from './ProgressPanel.svelte';

    let { sessionData, encryptionKey, myName = '', myId = '', oncomplete } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let chunks = $state(new Map());
    let bytesTransferred = $state(0);
    let chunksConfirmed = $state(0);
    let chunksAcknowledged = $state(0);
    let totalBytes = $state(0);
    let lastChunkIndex = $state(0);
    let fileInfo = $state(sessionData?.state?.file?.name ? sessionData.state.file : null);
    let uploadFinished = $state(sessionData?.state?.upload_finished || false);
    let errorMsg = $state('');

    // --- File System Access API (disk-based storage) ---
    const hasFSAccess = typeof globalThis.showSaveFilePicker === 'function';
    let fileHandle = $state(null);
    let writable = $state(null);
    let nextWriteIndex = $state(1);
    let chunkBuffer = $state(new Map());
    let writtenChunks = $state(new Set());
    let writableReady = null;  // Promise that resolves when writable is set up (or null if not using FS API)

    async function flushChunkToDisk(index, data) {
        if (!writable) return false;
        if (index === nextWriteIndex) {
            await writable.write(data);
            nextWriteIndex++;
            while (chunkBuffer.has(nextWriteIndex)) {
                await writable.write(chunkBuffer.get(nextWriteIndex));
                chunkBuffer.delete(nextWriteIndex);
                nextWriteIndex++;
            }
        } else {
            chunkBuffer.set(index, data);
        }
        writtenChunks = new Set([...writtenChunks, index]);
        return true;
    }

    let senderInfo = $state(sessionData?.members?.sender || null);
    let sessionExpirationIn = $state(sessionData?.state?.expiration_in || 0);

    $effect(() => {
        if (sessionExpirationIn <= 0) return;
        const interval = setInterval(() => {
            sessionExpirationIn = Math.max(0, sessionExpirationIn - 1);
        }, 1000);
        return () => clearInterval(interval);
    });
    let receivers = $state(sessionData?.members?.receivers || []);

    // Update own name in member list when changed via NameBadge
    $effect(() => {
        if (myName && myId) {
            const idx = receivers.findIndex(r => r.id === myId);
            if (idx !== -1 && receivers[idx].name !== myName) {
                receivers = receivers.map(r =>
                    r.id === myId ? { ...r, name: myName } : r
                );
            }
        }
    });

    let progress = $derived(totalBytes > 0 ? Math.min(1, bytesTransferred / totalBytes) : 0);

    async function promptSaveFile(name) {
        try {
            fileHandle = await window.showSaveFilePicker({ suggestedName: name });
            writable = await fileHandle.createWritable();
        } catch {
            fileHandle = null;
            writable = null;
        }
    }

    if (fileInfo) {
        totalBytes = fileInfo.size;
        if (hasFSAccess) {
            writableReady = promptSaveFile(fileInfo.name);
        }
    }

    // --- Event handlers ---

    async function onFileInfo(msg) {
        const d = msg.data || msg;
        fileInfo = { name: d.name, size: d.size };
        totalBytes = d.size;

        if (hasFSAccess && !writableReady) {
            writableReady = promptSaveFile(d.name);
        }
    }

    function onUploadFinished() {
        uploadFinished = true;
        checkIfDone();
    }

    function onNewReceiver(msg) {
        const r = msg.data || msg;
        receivers = [...receivers, { name: r.name, id: r.id, is_online: true }];
    }

    function onReceiverRemoved(msg) {
        const id = msg.data?.id ?? msg.id;
        receivers = receivers.filter(r => r.id !== id);
    }

    function onOnline(msg) {
        const d = msg.data || msg;
        if (senderInfo && senderInfo.id === d.id) {
            senderInfo = { ...senderInfo, is_online: d.status, _flash: Date.now() };
        } else {
            receivers = receivers.map(r =>
                r.id === d.id ? { ...r, is_online: d.status, _flash: Date.now() } : r
            );
        }
    }

    function onNameChanged(msg) {
        const d = msg.data || msg;
        if (senderInfo && senderInfo.id === d.id) {
            senderInfo = { ...senderInfo, name: d.name, _flash: Date.now() };
        } else {
            receivers = receivers.map(r =>
                r.id === d.id ? { ...r, name: d.name, _flash: Date.now() } : r
            );
        }
    }

    function onChunkDownload(msg) {
        const d = msg.data || msg;
        receivers = receivers.map(r =>
            r.id === d.id ? { ...r, current_chunk: d.index } : r
        );
        // Server acknowledged our confirm_chunk
        if (d.action === 'finished' && d.id === myId) {
            chunksAcknowledged++;
            checkIfDone();
        }
    }

    // Resync after WS reconnect — fetch any chunks we missed while disconnected
    function onReconnectInit(msg) {
        const state = msg.data?.state;
        if (!state) return;
        if (state.upload_finished) uploadFinished = true;
        const serverChunks = state.chunks;
        if (serverChunks && Array.isArray(serverChunks)) {
            (async () => {
                for (const c of serverChunks) {
                    if (c.index > highestKnownChunk) highestKnownChunk = c.index;
                    await fetchAndDecryptChunk(c.index);
                }
                checkIfDone();
            })();
        }
    }

    async function fetchAndDecryptChunk(index, retries = 3) {
        // Wait for save file dialog before downloading
        if (writableReady) await writableReady;
        if (writable ? writtenChunks.has(index) : chunks.has(index)) return;

        for (let attempt = 0; attempt < retries; attempt++) {
            try {
                const result = await downloadChunk(index);
                if (result.status === 200 && result.data) {
                    const encrypted = new Uint8Array(result.data);
                    const decrypted = decryptChunk(encrypted, encryptionKey);

                    if (writable) {
                        await flushChunkToDisk(index, decrypted);
                    } else {
                        chunks.set(index, decrypted);
                        chunks = new Map(chunks);
                    }
                    if (index > lastChunkIndex) lastChunkIndex = index;

                    if (!sendAction('confirm_chunk', { index })) {
                        await waitForConnection();
                        sendAction('confirm_chunk', { index });
                    }
                    chunksConfirmed++;
                    return;
                }
                if (result.status === 404) return; // chunk removed, skip
                await new Promise(r => setTimeout(r, 1000 * (attempt + 1)));
            } catch (err) {
                if (attempt === retries - 1) {
                    errorMsg = `Chunk ${index}: ${err.message}`;
                }
                await new Promise(r => setTimeout(r, 1000 * (attempt + 1)));
            }
        }
    }

    async function onNewChunk(msg) {
        const d = msg.data || msg;
        if (d.index > highestKnownChunk) highestKnownChunk = d.index;
        await fetchAndDecryptChunk(d.index);
        checkIfDone();
    }

    function onPersonalReceived(msg) {
        const d = msg.data || msg;
        // Server reports encrypted bytes; cap at file size for display
        bytesTransferred = Math.min(d.bytes, totalBytes);
        checkIfDone();
    }

    // --- Completion ---

    let completeHandled = false;

    function assembleFile() {
        if (!fileInfo || chunks.size === 0) return null;
        const sortedKeys = [...chunks.keys()].sort((a, b) => a - b);
        return new Blob(sortedKeys.map(k => chunks.get(k)));
    }

    async function finishWithBlob() {
        if (completeHandled) return;
        completeHandled = true;

        if (writable) {
            // File written to disk via File System Access API.
            // Show completion screen immediately with "finalizing" state.
            // Pass a promise so the completion screen can track progress.
            const w = writable;
            writable = null;
            const closePromise = w.close();
            oncomplete?.({ status: 'ok', blob: null, savedToDisk: true, closePromise });
        } else {
            const blob = assembleFile();
            oncomplete?.({ status: 'ok', blob });
        }
    }

    function checkIfDone() {
        if (completeHandled) return;
        const hasData = writable ? writtenChunks.size > 0 : chunks.size > 0;
        if (uploadFinished && hasData && noMorePendingChunks
            && chunksAcknowledged >= chunksConfirmed) {
            finishWithBlob();
        }
    }

    // Track if we're still expecting chunks
    let highestKnownChunk = $state(Math.max(
        sessionData?.state?.current_chunk || 0,
        ...(Array.isArray(sessionData?.state?.chunks) ? sessionData.state.chunks.map(c => c.index) : [0])
    ));
    let hasReceivedData = $derived(writable ? writtenChunks.size > 0 : chunks.size > 0);
    let noMorePendingChunks = $derived(uploadFinished && hasReceivedData && lastChunkIndex >= highestKnownChunk);

    function onSessionComplete(msg) {
        if (completeHandled) return;
        const d = msg.data || msg;
        const status = d.status || 'ok';

        if (status === 'ok') {
            finishWithBlob();
        } else {
            completeHandled = true;
            oncomplete?.({ status });
        }
    }

    function onWsClose() {
        if (completeHandled) return;
        setTimeout(() => {
            if (completeHandled) return;
            const hasData = writable ? writtenChunks.size > 0 : chunks.size > 0;
            if (uploadFinished && hasData) {
                finishWithBlob();
            } else {
                completeHandled = true;
                if (writable) { writable.close().catch(() => {}); writable = null; }
                oncomplete?.({ status: 'error' });
            }
        }, 500);
    }

    async function handleLeave() {
        disconnect();
        await leave();
        completeHandled = true;
        oncomplete?.({ status: 'left' });
    }

    // --- WS subscriptions ---

    $effect(() => {
        on('file_info', onFileInfo);
        on('upload_finished', onUploadFinished);
        on('new_chunk', onNewChunk);
        on('chunk_download', onChunkDownload);
        on('new_receiver', onNewReceiver);
        on('receiver_removed', onReceiverRemoved);
        on('name_changed', onNameChanged);
        on('complete', onSessionComplete);
        on('online', onOnline);
        on('personal_received', onPersonalReceived);
        on('close', onWsClose);
        on('start_init', onReconnectInit);

        return () => {
            off('file_info', onFileInfo);
            off('upload_finished', onUploadFinished);
            off('new_chunk', onNewChunk);
            off('chunk_download', onChunkDownload);
            off('new_receiver', onNewReceiver);
            off('receiver_removed', onReceiverRemoved);
            off('name_changed', onNameChanged);
            off('complete', onSessionComplete);
            off('online', onOnline);
            off('personal_received', onPersonalReceived);
            off('close', onWsClose);
            off('start_init', onReconnectInit);
        };
    });

    // Download chunks that already exist when we joined
    let initialFetchDone = false;
    $effect(() => {
        if (initialFetchDone) return;
        initialFetchDone = true;

        const existingChunks = sessionData?.state?.chunks;
        if (existingChunks && Array.isArray(existingChunks)) {
            (async () => {
                for (const c of existingChunks) {
                    await fetchAndDecryptChunk(c.index);
                }
            })();
        }
    });
</script>

<div class="session">
    <div class="sidebar">
        <MemberList
            sender={senderInfo}
            {receivers}
            isSender={false}
            {myId}
        />
        <SessionTimer remaining={sessionExpirationIn} />
        <button class="leave-btn" onclick={handleLeave}>
            {tt('leaveSession')}
        </button>
    </div>

    <div class="main">
        <ProgressPanel
            {fileInfo}
            {progress}
            {bytesTransferred}
            {totalBytes}
            isSender={false}
            bufferUsed={0}
            bufferMax={0}
            {uploadFinished}
            chunks={[]}
        />

        <div class="chunk-info">
            {tt('chunks')}: {chunksConfirmed} / {highestKnownChunk || '?'}
        </div>

        {#if !hasFSAccess}
            <div class="warning">
                {tt('fsWarning')}
                (<a href="https://github.com/askhatovich/putinqa" target="_blank" rel="noopener">{tt('fsWarningDesktop')}</a>)
            </div>
        {/if}

        {#if errorMsg}
            <div class="error">{errorMsg}</div>
        {/if}
    </div>
</div>

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

    .main {
        display: flex;
        flex-direction: column;
        gap: 1rem;
    }

    .chunk-info {
        color: #999;
        font-size: 0.8rem;
        text-align: center;
    }

    .warning {
        color: #f0a030;
        font-size: 0.8rem;
        padding: 0.5rem 0.75rem;
        background: rgba(240, 160, 48, 0.1);
        border-radius: 4px;
        line-height: 1.4;
    }

    .warning a {
        color: #f0a030;
        text-decoration: underline;
    }

    .warning a:hover {
        color: #eee;
    }

    .error {
        color: #e94560;
        font-size: 0.85rem;
        padding: 0.5rem;
        background: rgba(233, 69, 96, 0.1);
        border-radius: 4px;
    }

    .leave-btn {
        background: rgba(233, 69, 96, 0.15);
        color: #e94560;
        border: 1px solid #e94560;
        padding: 0.5rem 1rem;
        border-radius: 6px;
        cursor: pointer;
        font-size: 0.85rem;
        font-family: inherit;
        transition: background 0.15s;
    }

    .leave-btn:hover {
        background: rgba(233, 69, 96, 0.3);
    }

    @media (max-width: 640px) {
        .session {
            grid-template-columns: 1fr;
            max-width: 100%;
            padding: 0.75rem;
        }
    }
</style>
