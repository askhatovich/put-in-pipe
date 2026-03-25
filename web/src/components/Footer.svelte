<script>
    import { t, lang, setLang, subscribe } from '$lib/i18n.js';

    let { stats = null } = $props();

    let langTick = $state(0);

    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let currentLang = $derived((langTick, lang()));

    let statsText = $derived(() => {
        if (!stats) return '...';
        return `${tt('serverLoad')}: ${stats.current_session_count}/${stats.max_session_count} · ${tt('users')}: ${stats.current_user_count}/${stats.max_user_count}`;
    });
</script>

<footer>
    <span class="stats">{statsText()}</span>
    <span class="copy"><a href="https://github.com/askhatovich/put-in-pipe" target="_blank" rel="noopener">Put-In-Pipe</a>&#32;{#if stats?.version}{stats.version}&#32;{/if}&#32;&copy; 2026</span>
    <span class="lang">
        <button class:active={currentLang === 'en'} onclick={() => setLang('en')}>EN</button>
        <span class="sep">|</span>
        <button class:active={currentLang === 'ru'} onclick={() => setLang('ru')}>RU</button>
    </span>
</footer>

<style>
    footer {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 0.15rem;
        padding: 0.5rem 1rem;
        font-size: 0.75rem;
        color: #666;
        font-family: system-ui, -apple-system, sans-serif;
    }

    .copy a {
        color: #666;
        text-decoration: none;
    }

    .copy a:hover {
        color: #e94560;
    }

    .lang {
        display: flex;
        align-items: center;
        gap: 0.25rem;
    }

    .lang button {
        background: none;
        border: none;
        color: #666;
        cursor: pointer;
        padding: 0.1rem 0.25rem;
        font-size: 0.75rem;
        font-family: inherit;
    }

    .lang button:hover {
        color: #eee;
    }

    .lang button.active {
        color: #e94560;
    }

    .sep {
        color: #444;
    }
</style>
