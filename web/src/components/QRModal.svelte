<script>
    import QRCode from 'qrcode';

    let { url = '', onclose } = $props();

    let qrDataUrl = $state('');

    $effect(() => {
        if (url) {
            QRCode.toDataURL(url, { width: 280, margin: 2, color: { dark: '#eee', light: '#1a1a2e' } })
                .then(dataUrl => { qrDataUrl = dataUrl; })
                .catch(() => { qrDataUrl = ''; });
        }
    });

    function handleBackdrop(e) {
        if (e.target === e.currentTarget) onclose?.();
    }
</script>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="backdrop" onclick={handleBackdrop}>
    <div class="modal">
        <button class="close-btn" onclick={() => onclose?.()}>&times;</button>
        {#if qrDataUrl}
            <img src={qrDataUrl} alt="QR Code" class="qr-img" />
        {:else}
            <div class="loading">...</div>
        {/if}
    </div>
</div>

<style>
    .backdrop {
        position: fixed;
        inset: 0;
        background: rgba(0, 0, 0, 0.7);
        display: flex;
        align-items: center;
        justify-content: center;
        z-index: 200;
    }

    .modal {
        background: #16213e;
        border-radius: 12px;
        padding: 1.5rem;
        position: relative;
        box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
    }

    .close-btn {
        position: absolute;
        top: 0.5rem;
        right: 0.75rem;
        background: none;
        border: none;
        color: #999;
        font-size: 1.5rem;
        cursor: pointer;
        line-height: 1;
    }

    .close-btn:hover {
        color: #eee;
    }

    .qr-img {
        display: block;
        width: 280px;
        height: 280px;
        border-radius: 8px;
    }

    .loading {
        width: 280px;
        height: 280px;
        display: flex;
        align-items: center;
        justify-content: center;
        color: #999;
    }
</style>
