<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { status = 'ok', blob = null, fileName = '', onrestart } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let blobUrl = $state(null);

    $effect(() => {
        if (blob) {
            const url = URL.createObjectURL(blob);
            blobUrl = url;
            return () => URL.revokeObjectURL(url);
        } else {
            blobUrl = null;
        }
    });

    function statusMessage(s) {
        switch (s) {
            case 'ok': return tt('transferComplete');
            case 'timeout': return tt('transferTimeout');
            case 'sender_is_gone': return tt('senderGone');
            case 'no_receivers': return tt('noReceiversEnd');
            default: return tt('error');
        }
    }

    let isSuccess = $derived(status === 'ok');
</script>

<div class="complete-panel">
    <div class="icon" class:success={isSuccess} class:fail={!isSuccess}>
        {isSuccess ? '\u2713' : '\u2717'}
    </div>

    <h2 class:success-text={isSuccess} class:fail-text={!isSuccess}>
        {statusMessage(status)}
    </h2>

    {#if blob && blobUrl}
        <a class="download-btn" href={blobUrl} download={fileName}>
            {tt('downloadFile')}
        </a>
        {#if fileName}
            <div class="file-name">{fileName}</div>
        {/if}
    {/if}

    <button class="restart-btn" onclick={() => onrestart?.()}>
        {tt('startOver')}
    </button>
</div>

<style>
    .complete-panel {
        text-align: center;
        padding: 2rem;
        max-width: 400px;
        margin: 0 auto;
        font-family: system-ui, -apple-system, sans-serif;
    }

    .icon {
        font-size: 3rem;
        width: 80px;
        height: 80px;
        line-height: 80px;
        border-radius: 50%;
        margin: 0 auto 1rem;
    }

    .icon.success {
        background: rgba(76, 175, 80, 0.15);
        color: #4caf50;
    }

    .icon.fail {
        background: rgba(233, 69, 96, 0.15);
        color: #e94560;
    }

    h2 {
        font-size: 1.3rem;
        margin: 0 0 1.5rem 0;
    }

    .success-text {
        color: #4caf50;
    }

    .fail-text {
        color: #e94560;
    }

    .download-btn {
        display: inline-block;
        padding: 0.7rem 2rem;
        background: #4caf50;
        color: #fff;
        text-decoration: none;
        border-radius: 4px;
        font-size: 1rem;
        margin-bottom: 0.5rem;
        transition: background 0.2s;
    }

    .download-btn:hover {
        background: #43a047;
    }

    .file-name {
        color: #999;
        font-size: 0.85rem;
        margin-bottom: 1.5rem;
        word-break: break-all;
    }

    .restart-btn {
        display: block;
        width: 100%;
        padding: 0.6rem;
        background: #0f3460;
        color: #eee;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.95rem;
        margin-top: 1rem;
        font-family: inherit;
    }

    .restart-btn:hover {
        background: #1a4a80;
    }
</style>
