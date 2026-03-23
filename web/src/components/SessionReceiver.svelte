<script>
    import { t, subscribe } from '$lib/i18n.js';
    import { decryptChunk } from '$lib/crypto.js';
    import { downloadChunk } from '$lib/api.js';
    import { sendAction, on, off } from '$lib/ws.js';
    import MemberList from './MemberList.svelte';
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
    let totalBytes = $state(0);
    let lastChunkIndex = $state(0);
    let fileInfo = $state(sessionData?.state?.file?.name ? sessionData.state.file : null);
    let uploadFinished = $state(sessionData?.state?.upload_finished || false);
    let errorMsg = $state('');

    let senderInfo = $state(sessionData?.members?.sender || null);
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

    if (fileInfo) totalBytes = fileInfo.size;

    // --- Event handlers ---

    function onFileInfo(msg) {
        const d = msg.data || msg;
        fileInfo = { name: d.name, size: d.size };
        totalBytes = d.size;
    }

    function onUploadFinished() {
        uploadFinished = true;
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
    }

    async function fetchAndDecryptChunk(index) {
        if (chunks.has(index)) return;

        try {
            const result = await downloadChunk(index);
            if (result.status !== 200 || !result.data) {
                errorMsg = `Chunk ${index}: ${result.error || 'HTTP ' + result.status}`;
                return;
            }

            const encrypted = new Uint8Array(result.data);
            const decrypted = decryptChunk(encrypted, encryptionKey);

            chunks.set(index, decrypted);
            chunks = new Map(chunks);
            if (index > lastChunkIndex) lastChunkIndex = index;

            sendAction('confirm_chunk', { index });
            chunksConfirmed++;
        } catch (err) {
            errorMsg = `Chunk ${index}: ${err.message}`;
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

    function finishWithBlob() {
        if (completeHandled) return;
        completeHandled = true;
        const blob = assembleFile();
        console.log('[Receiver] assembled blob:', blob?.size, 'expected:', totalBytes);
        oncomplete?.({ status: 'ok', blob });
    }

    function checkIfDone() {
        if (completeHandled) return;
        // All data received: upload finished and we have chunks
        if (uploadFinished && chunks.size > 0 && noMorePendingChunks) {
            finishWithBlob();
        }
    }

    // Track if we're still expecting chunks
    let highestKnownChunk = $state(Math.max(
        sessionData?.state?.current_chunk || 0,
        ...(Array.isArray(sessionData?.state?.chunks) ? sessionData.state.chunks.map(c => c.index) : [0])
    ));
    let noMorePendingChunks = $derived(uploadFinished && chunks.size > 0 && lastChunkIndex >= highestKnownChunk);

    function onSessionComplete(msg) {
        if (completeHandled) return;
        const d = msg.data || msg;
        const status = d.status || 'ok';
        console.log('[Receiver] complete event:', status);

        if (status === 'ok') {
            finishWithBlob();
        } else {
            completeHandled = true;
            oncomplete?.({ status });
        }
    }

    function onWsClose() {
        if (completeHandled) return;
        console.log('[Receiver] WS closed', { uploadFinished, chunks: chunks.size, lastChunkIndex, highestKnownChunk });
        setTimeout(() => {
            if (completeHandled) return;
            // If upload finished and we have data, assemble what we have
            if (uploadFinished && chunks.size > 0) {
                finishWithBlob();
            } else {
                completeHandled = true;
                oncomplete?.({ status: 'error' });
            }
        }, 500);
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
        />
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
