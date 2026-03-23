<script>
    import { t, subscribe } from '$lib/i18n.js';
    import { confirmIdentity } from '$lib/api.js';

    let { captchaData, name, onsolved, onretry } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    let answer = $state('');
    let timeLeft = $state(captchaData.captcha_lifetime);
    let errorMsg = $state('');
    let submitting = $state(false);

    $effect(() => {
        timeLeft = captchaData.captcha_lifetime;
        const interval = setInterval(() => {
            timeLeft--;
            if (timeLeft <= 0) {
                clearInterval(interval);
                onretry?.();
            }
        }, 1000);
        return () => clearInterval(interval);
    });

    async function handleSubmit() {
        if (!answer.trim() || submitting) return;
        submitting = true;
        errorMsg = '';

        const result = await confirmIdentity({
            name,
            client_id: captchaData.client_id,
            captcha_token: captchaData.captcha_token,
            captcha_answer: answer.trim(),
        });

        if (result.status === 201) {
            onsolved?.({
                name,
                id: result.data?.id,
                publicId: result.data?.public_id,
            });
        } else {
            errorMsg = result.data?.message || result.error || t('error');
            submitting = false;
            onretry?.();
        }
    }
</script>

<div class="captcha-panel">
    <h3>{tt('captchaTitle')}</h3>
    <img
        src="data:image/png;base64,{captchaData.captcha_image}"
        alt="captcha"
    />
    <div class="timer">
        {tt('captchaExpires')}: {timeLeft}{tt('seconds')}
    </div>
    <form onsubmit={(e) => { e.preventDefault(); handleSubmit(); }}>
        <input
            type="text"
            bind:value={answer}
            maxlength={captchaData.captcha_answer_length}
            placeholder={tt('captchaTitle')}
            autocomplete="off"
        />
        <button type="submit" disabled={!answer.trim() || submitting}>
            {tt('captchaSubmit')}
        </button>
    </form>
    {#if errorMsg}
        <div class="error">{errorMsg}</div>
    {/if}
</div>

<style>
    .captcha-panel {
        text-align: center;
        padding: 1.5rem;
        background: #16213e;
        border-radius: 8px;
        max-width: 320px;
        margin: 0 auto;
        font-family: system-ui, -apple-system, sans-serif;
    }

    h3 {
        color: #eee;
        margin: 0 0 1rem 0;
        font-size: 1.1rem;
    }

    img {
        image-rendering: pixelated;
        width: 200px;
        height: 200px;
        display: block;
        margin: 0 auto;
        border-radius: 4px;
    }

    .timer {
        color: #999;
        font-size: 0.85rem;
        margin: 0.75rem 0;
    }

    form {
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
    }

    input {
        padding: 0.5rem;
        border: 1px solid #0f3460;
        border-radius: 4px;
        background: #1a1a2e;
        color: #eee;
        font-size: 1rem;
        text-align: center;
        letter-spacing: 0.15em;
        font-family: monospace;
        outline: none;
    }

    input:focus {
        border-color: #e94560;
    }

    button {
        padding: 0.5rem 1rem;
        background: #e94560;
        color: #fff;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.95rem;
        font-family: inherit;
    }

    button:disabled {
        opacity: 0.5;
        cursor: not-allowed;
    }

    button:hover:not(:disabled) {
        background: #d63450;
    }

    .error {
        color: #e94560;
        margin-top: 0.5rem;
        font-size: 0.85rem;
    }
</style>
