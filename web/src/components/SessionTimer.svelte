<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { expirationIn = 0 } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let remaining = $state(expirationIn);

    $effect(() => {
        remaining = expirationIn;
        const interval = setInterval(() => {
            remaining = Math.max(0, remaining - 1);
        }, 1000);
        return () => clearInterval(interval);
    });

    let displayTime = $derived(() => {
        const m = Math.floor(remaining / 60);
        const s = remaining % 60;
        const sp = s < 10 ? '0' : '';
        if (m > 0) return `${m}${tt('minutes')} ${sp}${s}${tt('seconds')}`;
        return `${s}${tt('seconds')}`;
    });
</script>

<div class="session-timer">
    {tt('sessionExpires')} {displayTime()}
</div>

<style>
    .session-timer {
        color: #666;
        font-size: 0.75rem;
        text-align: center;
        padding: 0.4rem 0;
    }
</style>
