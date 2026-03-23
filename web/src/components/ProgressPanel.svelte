<script>
    import { t, subscribe } from '$lib/i18n.js';

    let {
        fileInfo = null,
        progress = 0,
        bytesTransferred = 0,
        totalBytes = 0,
        isSender = false,
        bufferUsed = 0,
        bufferMax = 0,
        uploadFinished = false,
        chunks = [],
        onfileselect,
    } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    function formatBytes(bytes) {
        if (bytes === 0) return '0 B';
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
    }

    let progressPercent = $derived(Math.round(progress * 100));

    function handleFileChange(e) {
        const file = e.target.files?.[0];
        if (file) {
            onfileselect?.(file);
        }
    }
</script>

<div class="progress-panel">
    {#if !fileInfo && isSender}
        <div class="file-picker">
            <label class="pick-btn">
                {tt('selectFile')}
                <input type="file" onchange={handleFileChange} />
            </label>
        </div>
    {:else if !fileInfo && !isSender}
        <div class="waiting">{tt('waitingForFile')}</div>
    {:else if fileInfo}
        <div class="file-info">
            <span class="file-name">{fileInfo.name}</span>
            <span class="file-size">{formatBytes(fileInfo.size)}</span>
        </div>

        <div class="bar-track">
            <div class="bar-fill" style="width: {progressPercent}%"></div>
        </div>
        <div class="bar-label">{progressPercent}%</div>

        <div class="transfer-info">
            {formatBytes(bytesTransferred)} / {formatBytes(totalBytes)}
        </div>

        {#if isSender}
            <div class="buffer-info">
                {tt('bufferStatus')}: {bufferUsed}/{bufferMax} {tt('chunks')}
            </div>
        {/if}

        {#if uploadFinished}
            <div class="finished-note">{tt('transferComplete')}</div>
        {/if}
    {/if}
</div>

<style>
    .progress-panel {
        background: #16213e;
        border-radius: 8px;
        padding: 1rem;
        font-family: system-ui, -apple-system, sans-serif;
    }

    .file-picker {
        text-align: center;
        padding: 2rem 0;
    }

    .pick-btn {
        display: inline-block;
        padding: 0.7rem 1.5rem;
        background: #0f3460;
        color: #eee;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.95rem;
        transition: background 0.2s;
    }

    .pick-btn:hover {
        background: #1a4a80;
    }

    .pick-btn input {
        display: none;
    }

    .waiting {
        color: #999;
        text-align: center;
        padding: 2rem 0;
        font-style: italic;
    }

    .file-info {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 0.75rem;
    }

    .file-name {
        color: #eee;
        font-size: 0.95rem;
        word-break: break-all;
        flex: 1;
        margin-right: 0.5rem;
    }

    .file-size {
        color: #999;
        font-size: 0.85rem;
        white-space: nowrap;
    }

    .bar-track {
        width: 100%;
        height: 8px;
        background: #1a1a2e;
        border-radius: 4px;
        overflow: hidden;
    }

    .bar-fill {
        height: 100%;
        background: #e94560;
        border-radius: 4px;
        transition: width 0.3s ease;
    }

    .bar-label {
        color: #999;
        font-size: 0.8rem;
        text-align: right;
        margin-top: 0.25rem;
    }

    .transfer-info {
        color: #eee;
        font-size: 0.85rem;
        margin-top: 0.5rem;
    }

    .buffer-info {
        color: #999;
        font-size: 0.8rem;
        margin-top: 0.4rem;
    }

    .finished-note {
        color: #4caf50;
        font-size: 0.85rem;
        margin-top: 0.5rem;
    }
</style>
