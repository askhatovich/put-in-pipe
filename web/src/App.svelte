<script>
    import { t, subscribe as i18nSubscribe, getOrGenerateName, saveName } from '$lib/i18n.js';
    import { initCrypto, generateKey, base64urlToKey } from '$lib/crypto.js';
    import { getStatistics, getMyInfo, leave, requestIdentity, createSession, joinSession } from '$lib/api.js';
    import { connect, disconnect, on, off, sendAction as wsSendAction, isConnected } from '$lib/ws.js';
    import { parseAnchor, clearAnchor } from '$lib/url.js';

    import Footer from './components/Footer.svelte';
    import NameBadge from './components/NameBadge.svelte';
    import Captcha from './components/Captcha.svelte';
    import EntryScreen from './components/EntryScreen.svelte';
    import SessionSender from './components/SessionSender.svelte';
    import SessionReceiver from './components/SessionReceiver.svelte';
    import TransferComplete from './components/TransferComplete.svelte';

    let langTick = $state(0);
    i18nSubscribe(() => langTick++);
    function tt(key) { langTick; return t(key); }

    // --- IndexedDB helpers for cross-reload blob passing ---
    function openDlDB() {
        return new Promise((resolve, reject) => {
            const req = indexedDB.open('pip-dl', 1);
            req.onupgradeneeded = () => req.result.createObjectStore('f');
            req.onsuccess = () => resolve(req.result);
            req.onerror = () => reject(req.error);
        });
    }
    async function savePendingDl(blob, name) {
        const db = await openDlDB();
        await new Promise((res, rej) => {
            const tx = db.transaction('f', 'readwrite');
            tx.objectStore('f').put({ blob, name, ts: Date.now() }, 'p');
            tx.oncomplete = () => { db.close(); res(); };
            tx.onerror = () => { db.close(); rej(tx.error); };
        });
    }
    async function loadPendingDl() {
        try {
            const db = await openDlDB();
            return await new Promise((res) => {
                const tx = db.transaction('f', 'readonly');
                const r = tx.objectStore('f').get('p');
                r.onsuccess = () => { db.close(); res(r.result || null); };
                r.onerror = () => { db.close(); res(null); };
            });
        } catch { return null; }
    }
    async function clearPendingDl() {
        try {
            const db = await openDlDB();
            const tx = db.transaction('f', 'readwrite');
            tx.objectStore('f').delete('p');
            tx.oncomplete = () => db.close();
        } catch {}
    }

    // 'loading', 'entry', 'file_select', 'captcha', 'connecting', 'sender', 'receiver', 'complete'
    let screen = $state('loading');
    let stats = $state(null);
    let errorMsg = $state('');
    let userName = $state('');

    let captchaData = $state(null);

    let sessionData = $state(null);
    let encryptionKey = $state(null);
    let pendingRole = $state(null);
    let pendingFile = $state(null);
    let myPublicId = $state('');

    let completeStatus = $state('');
    let completeBlob = $state(null);
    let completeFileName = $state('');
    let finalizing = $state(false);

    let statsInterval;
    async function pollStats() {
        try {
            const res = await getStatistics();
            if (res.status === 200) stats = res.data;
        } catch (e) { /* ignore */ }
    }

    function handleNameChange(newName) {
        userName = newName;
        saveName(newName);
        if (isConnected()) {
            wsSendAction('new_name', { name: newName });
        }
    }

    async function init() {
        await initCrypto();

        // Check for pending download saved before reload
        const pending = await loadPendingDl();
        if (pending && (Date.now() - pending.ts) < 60000) {
            await clearPendingDl();
            completeBlob = pending.blob;
            completeFileName = pending.name;
            completeStatus = 'ok';
            userName = getOrGenerateName();
            pollStats();
            statsInterval = setInterval(pollStats, 5000);
            screen = 'complete';
            return;
        }
        await clearPendingDl();

        pollStats();
        statsInterval = setInterval(pollStats, 5000);

        userName = getOrGenerateName();

        const anchor = parseAnchor();
        if (anchor) {
            if (anchor.error) {
                errorMsg = anchor.error;
                clearAnchor();
                screen = 'entry';
                return;
            }
            pendingRole = 'receiver';
            encryptionKey = base64urlToKey(anchor.key);
            screen = 'connecting';
            await identifyAndProceed(userName, 'receiver');
            return;
        }

        screen = 'entry';
    }

    // --- Identity ---

    async function ensureIdentified(name) {
        const info = await getMyInfo();
        if (info.status === 200 && info.data) {
            if (info.data.id) myPublicId = info.data.id;
            if (info.data.session && info.data.session.length > 0) {
                await leave();
                return await requestNewIdentity(name);
            }
            return true;
        }
        return await requestNewIdentity(name);
    }

    async function requestNewIdentity(name) {
        const res = await requestIdentity(name);
        if (res.status === 201) {
            if (res.data?.id) myPublicId = res.data.id;
            return true;
        }
        if (res.status === 400) return true;
        if (res.status === 401 && res.data && res.data.captcha_image) {
            captchaData = res.data;
            userName = name;
            screen = 'captcha';
            return false;
        }
        if (res.status === 503) {
            errorMsg = res.data || 'Server is full, try again later';
            return false;
        }
        errorMsg = res.data || res.error || t('error');
        return false;
    }

    async function identifyAndProceed(name, role) {
        errorMsg = '';
        userName = name;
        saveName(name);
        pendingRole = role;

        const identified = await ensureIdentified(name);
        if (!identified) return;

        await proceedAfterIdentity();
    }

    // --- Entry handlers ---

    async function handleJoinLink({ id, key, encryption }) {
        const SUPPORTED = ['xchacha20-poly1305'];
        if (!SUPPORTED.includes(encryption)) {
            errorMsg = `Unsupported encryption: ${encryption}`;
            return;
        }
        // Set anchor so startReceiver can parse it
        location.hash = `id=${id}&encryption=${encryption}&key=${key}`;
        pendingRole = 'receiver';
        encryptionKey = base64urlToKey(key);
        screen = 'connecting';
        await identifyAndProceed(userName, 'receiver');
    }

    async function handleFileSelected(file) {
        pendingFile = file;
        pendingRole = 'sender';
        screen = 'connecting';
        await identifyAndProceed(userName, 'sender');
    }

    function handleCaptchaSolved() {
        captchaData = null;
        proceedAfterIdentity();
    }

    function handleCaptchaRetry() {
        requestNewIdentity(userName);
    }

    // --- Session start ---

    async function proceedAfterIdentity() {
        if (pendingRole === 'sender') {
            await startSender();
        } else {
            await startReceiver();
        }
    }

    async function startSender() {
        const res = await createSession();
        if (res.status !== 201) {
            errorMsg = res.data || res.error || t('error');
            screen = 'entry';
            return;
        }

        encryptionKey = generateKey();

        try {
            await connect();
        } catch (e) {
            errorMsg = t('error') + ': WebSocket';
            screen = 'entry';
            return;
        }

        on('start_init', (msg) => {
            sessionData = msg.data;
            screen = 'sender';
        });
    }

    async function startReceiver() {
        const anchor = parseAnchor();
        if (!anchor || anchor.error) {
            errorMsg = anchor?.error || t('error');
            screen = 'entry';
            return;
        }

        const res = await joinSession(anchor.id);
        if (res.status !== 202) {
            errorMsg = res.data || res.error || t('error');
            screen = 'entry';
            return;
        }

        encryptionKey = base64urlToKey(anchor.key);

        try {
            await connect();
        } catch (e) {
            errorMsg = t('error') + ': WebSocket';
            screen = 'entry';
            return;
        }

        on('start_init', (msg) => {
            sessionData = msg.data;
            screen = 'receiver';
        });
    }

    function needsReloadForBlobDownload() {
        const ua = navigator.userAgent;
        // Mobile Chromium-based browsers (not Firefox) have issues with blob URL downloads
        return /Android/.test(ua) && !/Firefox/.test(ua);
    }

    async function handleComplete({ status, blob, savedToDisk, closePromise = null }) {
        disconnect();
        // Receiver: delay leave() so sender sees checkmark for a few seconds
        if (pendingRole === 'receiver' && status === 'ok') {
            setTimeout(() => leave().catch(() => {}), 5000);
        } else if (pendingRole === 'receiver') {
            leave().catch(() => {});
        }

        // Receiver left voluntarily — return to entry screen
        if (status === 'left') {
            clearAnchor();
            handleRestart();
            return;
        }

        const name = sessionData?.state?.file?.name || 'download';

        // File saved to disk via File System Access API — finalize
        if (status === 'ok' && savedToDisk) {
            if (closePromise) {
                finalizing = true;
                const beforeUnload = (e) => { e.preventDefault(); };
                window.addEventListener('beforeunload', beforeUnload);
                await closePromise;
                window.removeEventListener('beforeunload', beforeUnload);
                finalizing = false;
            }
            completeStatus = status;
            completeBlob = null;
            completeFileName = name;
            screen = 'complete';
            return;
        }

        // Only mobile Chromium needs the reload trick for blob downloads
        if (status === 'ok' && blob && needsReloadForBlobDownload()) {
            try {
                await savePendingDl(blob, name);
                clearAnchor();
                window.location.reload();
                return;
            } catch { /* IndexedDB unavailable, fall through */ }
        }

        completeStatus = status;
        completeBlob = blob || null;
        completeFileName = name;
        screen = 'complete';
    }

    function handleRestart() {
        clearAnchor();
        sessionData = null;
        encryptionKey = null;
        captchaData = null;
        pendingFile = null;
        completeStatus = '';
        completeBlob = null;
        completeFileName = '';
        errorMsg = '';
        pendingRole = null;
        screen = 'entry';
    }

    // Run init once on mount, not inside $effect to avoid re-runs on state changes
    init();

    // Cleanup on unmount
    $effect(() => {
        return () => {
            clearInterval(statsInterval);
            disconnect();
        };
    });
</script>

<div class="app">
    <NameBadge bind:name={userName} onchange={handleNameChange} />

    <main>
        {#if screen === 'loading' || screen === 'connecting'}
            <div class="center">
                <div class="spinner"></div>
            </div>
        {:else if screen === 'entry'}
            <EntryScreen name={userName} onfileselected={handleFileSelected} onjoinlink={handleJoinLink} />
            {#if errorMsg}
                <p class="error">{errorMsg}</p>
            {/if}
        {:else if screen === 'captcha'}
            <Captcha
                captchaData={captchaData}
                name={userName}
                onsolved={handleCaptchaSolved}
                onretry={handleCaptchaRetry}
            />
        {:else if screen === 'sender'}
            <SessionSender
                sessionData={sessionData}
                encryptionKey={encryptionKey}
                initialFile={pendingFile}
                myName={userName}
                myId={myPublicId}
                oncomplete={handleComplete}
            />
        {:else if screen === 'receiver'}
            <SessionReceiver
                sessionData={sessionData}
                encryptionKey={encryptionKey}
                myName={userName}
                myId={myPublicId}
                oncomplete={handleComplete}
            />
        {:else if screen === 'complete'}
            <TransferComplete
                status={completeStatus}
                blob={completeBlob}
                fileName={completeFileName}
                onrestart={handleRestart}
            />
        {/if}

        {#if finalizing}
            <div class="finalizing-overlay">
                <div class="finalizing-box">
                    <div class="spinner"></div>
                    <div class="finalizing-text">{tt('finalizing')}</div>
                    <div class="finalizing-warn">{tt('finalizingWarn')}</div>
                </div>
            </div>
        {/if}
    </main>

    <Footer {stats} />
</div>

<style>
    :global(html, body) {
        margin: 0;
        padding: 0;
        background: #1a1a2e;
        color: #eee;
        font-family: system-ui, -apple-system, sans-serif;
        min-height: 100vh;
        overflow-x: hidden;
    }

    :global(*) {
        box-sizing: border-box;
    }

    .app {
        display: flex;
        flex-direction: column;
        min-height: 100vh;
    }

    main {
        flex: 1;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        padding: 2rem;
        max-width: 100%;
        overflow-x: hidden;
    }

    @media (max-width: 480px) {
        main {
            padding: 1rem 0.75rem;
        }
    }

    .center {
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .spinner {
        width: 40px;
        height: 40px;
        border: 3px solid #333;
        border-top-color: #e94560;
        border-radius: 50%;
        animation: spin 0.8s linear infinite;
    }

    @keyframes spin {
        to { transform: rotate(360deg); }
    }

    .error {
        color: #e94560;
        text-align: center;
        margin-top: 1rem;
        font-size: 0.9rem;
    }


    .finalizing-overlay {
        position: fixed;
        inset: 0;
        background: rgba(26, 26, 46, 0.95);
        display: flex;
        align-items: center;
        justify-content: center;
        z-index: 1000;
    }

    .finalizing-box {
        text-align: center;
    }

    .spinner {
        width: 40px;
        height: 40px;
        border: 3px solid #333;
        border-top-color: #e94560;
        border-radius: 50%;
        animation: spin 0.8s linear infinite;
        margin: 0 auto 1rem;
    }

    @keyframes spin {
        to { transform: rotate(360deg); }
    }

    .finalizing-text {
        color: #eee;
        font-size: 1.1rem;
        margin-bottom: 0.5rem;
    }

    .finalizing-warn {
        color: #e94560;
        font-size: 0.85rem;
    }
</style>
