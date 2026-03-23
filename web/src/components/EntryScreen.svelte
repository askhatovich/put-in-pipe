<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { name: initialName = '', onstart, onjoinlink } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let name = $state(initialName);
    let linkInput = $state('');
    let linkError = $state('');

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

    <div class="buttons">
        <button
            class="btn-send"
            onclick={() => onstart?.({ role: 'sender', name: name.trim() })}
        >
            {tt('sendFile')}
        </button>
    </div>

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
        margin: 0 auto;
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

    .buttons {
        display: flex;
        gap: 0.75rem;
        justify-content: center;
    }

    .buttons button {
        padding: 0.7rem 2rem;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.95rem;
        font-family: inherit;
        transition: background 0.2s;
    }

    .btn-send {
        background: #0f3460;
        color: #eee;
    }

    .btn-send:hover {
        background: #1a4a80;
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
