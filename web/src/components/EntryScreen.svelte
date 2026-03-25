<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { name: initialName = '', onfileselected, onjoinlink } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let name = $state(initialName);
    let linkInput = $state('');
    let linkError = $state('');
    let dragging = $state(false);

    function handleFiles(files) {
        const f = files?.[0];
        if (f) onfileselected?.(f);
    }

    function handleDrop(e) {
        e.preventDefault();
        dragging = false;
        handleFiles(e.dataTransfer?.files);
    }

    function handleDragOver(e) {
        e.preventDefault();
        dragging = true;
    }

    function handleJoinLink() {
        linkError = '';
        try {
            const url = new URL(linkInput.trim());
            const hash = url.hash.slice(1);
            if (!hash) { linkError = 'No anchor in URL'; return; }
            const params = new URLSearchParams(hash);
            const id = params.get('id');
            const key = params.get('key');
            const encryption = params.get('encryption');
            if (!id || !key) { linkError = 'Missing id or key'; return; }
            onjoinlink?.({ id, key, encryption: encryption || 'xchacha20-poly1305' });
        } catch {
            linkError = 'Invalid URL';
        }
    }
</script>

<div class="entry">
    <h1 class="title">Put-In-Pipe</h1>
    <p class="subtitle">{tt('encrypted')}</p>

    <label
        class="drop-zone"
        class:dragging
        role="button"
        tabindex="0"
        ondrop={handleDrop}
        ondragover={handleDragOver}
        ondragleave={() => dragging = false}
    >
        <input type="file" onchange={(e) => handleFiles(e.target.files)} />
        <span class="drop-icon">+</span>
        <span class="drop-text">{tt('sendFile')}</span>
    </label>

    <div class="divider"></div>

    <div class="link-section">
        <label>{tt('pasteLink')}</label>
        <div class="link-row">
            <input
                type="text"
                bind:value={linkInput}
                placeholder={tt('pasteLinkPlaceholder')}
                onkeydown={(e) => { if (e.key === 'Enter' && linkInput.trim()) handleJoinLink(); }}
            />
            <button
                class="btn-join"
                disabled={!linkInput.trim()}
                onclick={handleJoinLink}
            >
                {tt('joinFromLink')}
            </button>
        </div>
        {#if linkError}
            <div class="link-error">{linkError}</div>
        {/if}
    </div>
</div>

<style>
    .entry {
        text-align: center;
        padding: 2rem 1rem;
        max-width: 440px;
        width: 100%;
        margin: 0 auto;
        box-sizing: border-box;
    }

    .title {
        color: #eee;
        font-size: 2rem;
        margin: 0 0 0.25rem 0;
    }

    .subtitle {
        color: #999;
        font-size: 0.9rem;
        margin: 0 0 2rem 0;
    }

    .drop-zone {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 0.5rem;
        padding: 2rem;
        border: 2px dashed #0f3460;
        border-radius: 8px;
        cursor: pointer;
        transition: border-color 0.2s, background 0.2s;
        background: transparent;
    }

    .drop-zone:hover, .drop-zone.dragging {
        border-color: #e94560;
        background: rgba(233, 69, 96, 0.05);
    }

    .drop-zone input[type="file"] {
        display: none;
    }

    .drop-icon {
        font-size: 2rem;
        color: #0f3460;
        line-height: 1;
        transition: color 0.2s;
    }

    .drop-zone:hover .drop-icon, .drop-zone.dragging .drop-icon {
        color: #e94560;
    }

    .drop-text {
        color: #999;
        font-size: 0.9rem;
    }

    .divider {
        margin: 1.5rem 0;
        border-top: 1px solid #0f3460;
    }

    .link-section {
        text-align: left;
    }

    .link-section label {
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
        min-width: 0;
        padding: 0.5rem 0.75rem;
        background: #16213e;
        border: 1px solid #0f3460;
        border-radius: 4px;
        color: #eee;
        font-size: 0.85rem;
        font-family: inherit;
        outline: none;
    }

    .link-row input:focus {
        border-color: #e94560;
    }

    .btn-join {
        padding: 0.5rem 1rem;
        background: #0f3460;
        color: #eee;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.85rem;
        font-family: inherit;
        white-space: nowrap;
    }

    .btn-join:hover:not(:disabled) {
        background: #1a4a80;
    }

    .btn-join:disabled {
        opacity: 0.4;
        cursor: not-allowed;
    }

    .link-error {
        color: #e94560;
        font-size: 0.8rem;
        margin-top: 0.4rem;
    }
</style>
