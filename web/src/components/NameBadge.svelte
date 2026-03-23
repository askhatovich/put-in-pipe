<script>
    import { saveName } from '$lib/i18n.js';

    let { name = $bindable(), onchange } = $props();

    let editing = $state(false);
    let inputValue = $state('');
    let inputEl;

    function startEditing() {
        inputValue = name;
        editing = true;
    }

    function finishEditing() {
        const trimmed = inputValue.trim();
        if (trimmed && trimmed !== name) {
            name = trimmed;
            saveName(trimmed);
            onchange?.(trimmed);
        }
        editing = false;
    }

    function handleKeydown(e) {
        if (e.key === 'Enter') finishEditing();
        if (e.key === 'Escape') editing = false;
    }

    $effect(() => {
        if (editing && inputEl) inputEl.focus();
    });
</script>

<div class="name-badge">
    {#if editing}
        <input
            bind:this={inputEl}
            bind:value={inputValue}
            onblur={finishEditing}
            onkeydown={handleKeydown}
            maxlength="20"
            class="name-input"
        />
    {:else}
        <button class="name-display" onclick={startEditing} title="Click to edit">
            {name}
        </button>
    {/if}
</div>

<style>
    .name-badge {
        position: fixed;
        top: 0.75rem;
        right: 0.75rem;
        z-index: 100;
    }

    .name-display {
        background: #16213e;
        border: 1px solid #0f3460;
        border-radius: 4px;
        color: #ccc;
        padding: 0.3rem 0.7rem;
        font-size: 0.85rem;
        cursor: pointer;
        font-family: inherit;
        transition: border-color 0.2s;
    }

    .name-display:hover {
        border-color: #e94560;
        color: #eee;
    }

    .name-input {
        background: #16213e;
        border: 1px solid #e94560;
        border-radius: 4px;
        color: #eee;
        padding: 0.3rem 0.7rem;
        font-size: 0.85rem;
        font-family: inherit;
        outline: none;
        width: 140px;
    }
</style>
