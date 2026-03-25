<script>
    import { t, subscribe } from '$lib/i18n.js';

    let { sender = null, receivers = [], isSender = false, myId = '', onkick } = $props();

    let langTick = $state(0);
    $effect(() => {
        const unsub = subscribe(() => { langTick++; });
        return unsub;
    });
    function tt(key) { langTick; return t(key); }

    // Track flash state per member ID
    let flashIds = $state(new Set());

    function triggerFlash(id) {
        flashIds = new Set([...flashIds, id]);
        setTimeout(() => {
            flashIds = new Set([...flashIds].filter(x => x !== id));
        }, 1200);
    }

    // Watch for _flash changes on sender
    let lastSenderFlash = 0;
    $effect(() => {
        if (sender?._flash && sender._flash !== lastSenderFlash) {
            lastSenderFlash = sender._flash;
            triggerFlash(sender.id);
        }
    });

    // Watch for _flash changes on receivers
    let lastReceiverFlashes = $state({});
    $effect(() => {
        for (const r of receivers) {
            if (r._flash && r._flash !== (lastReceiverFlashes[r.id] || 0)) {
                lastReceiverFlashes[r.id] = r._flash;
                triggerFlash(r.id);
            }
        }
    });
</script>

<div class="member-list">
    <div class="section">
        <h4>{tt('sender')}</h4>
        {#if sender}
            <div class="member" class:flash={flashIds.has(sender.id)}>
                <span class="dot" class:online={sender.is_online}></span>
                <span class="name" class:me={isSender}>{sender.name}</span>
            </div>
        {:else}
            <div class="muted">...</div>
        {/if}
    </div>

    <div class="section">
        <h4>{tt('receivers')}</h4>
        {#if receivers.length === 0}
            <div class="muted">{tt('noReceivers')}</div>
        {:else}
            {#each receivers as receiver (receiver.id)}
                <div class="member" class:flash={flashIds.has(receiver.id)}>
                    {#if receiver.done}
                        <span class="checkmark">&#10003;</span>
                    {:else}
                        <span class="dot" class:online={receiver.is_online}></span>
                    {/if}
                    <span class="name" class:me={receiver.id === myId}>{receiver.name}</span>
                    <span class="chunk-info">#{receiver.current_chunk || 0}</span>
                    {#if isSender}
                        <button class="kick-btn" onclick={() => onkick?.({ id: receiver.id })}>
                            &times;
                        </button>
                    {/if}
                </div>
            {/each}
        {/if}
    </div>
</div>

<style>
    .member-list {
        background: #16213e;
        border-radius: 8px;
        padding: 1rem;
        font-family: system-ui, -apple-system, sans-serif;
    }

    .section {
        margin-bottom: 1rem;
    }

    .section:last-child {
        margin-bottom: 0;
    }

    h4 {
        color: #999;
        font-size: 0.8rem;
        text-transform: uppercase;
        letter-spacing: 0.05em;
        margin: 0 0 0.5rem 0;
    }

    .member {
        display: flex;
        align-items: center;
        gap: 0.5rem;
        padding: 0.35rem 0.4rem;
        color: #eee;
        font-size: 0.9rem;
        border-radius: 4px;
        transition: background 0.3s;
    }

    .member.flash {
        animation: member-flash 1.2s ease-out;
    }

    @keyframes member-flash {
        0% { background: rgba(233, 69, 96, 0.3); }
        100% { background: transparent; }
    }

    .dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background: #555;
        flex-shrink: 0;
        transition: background 0.3s;
    }

    .dot.online {
        background: #4caf50;
    }

    .name {
        flex: 1;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .name.me {
        color: #7dcea0;
    }

    .chunk-info {
        color: #999;
        font-size: 0.75rem;
        font-family: monospace;
    }

    .checkmark {
        color: #4caf50;
        font-size: 0.9rem;
        font-weight: bold;
        flex-shrink: 0;
        width: 8px;
        text-align: center;
    }

    .kick-btn {
        background: none;
        border: none;
        color: #e94560;
        cursor: pointer;
        font-size: 1.1rem;
        padding: 0 0.25rem;
        line-height: 1;
    }

    .kick-btn:hover {
        color: #ff6b81;
    }

    .muted {
        color: #666;
        font-size: 0.85rem;
        font-style: italic;
    }
</style>
