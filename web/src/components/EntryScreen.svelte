<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { name: initialName = '', onstart } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let name = $state(initialName);
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
</div>

<style>
    .entry {
        text-align: center;
        padding: 2rem 1rem;
        max-width: 400px;
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
</style>
