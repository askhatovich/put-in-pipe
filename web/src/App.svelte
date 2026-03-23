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

    function handleSendClicked() {
        screen = 'file_select';
    }

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

    async function handleStart({ role, name }) {
        if (role === 'sender') {
            handleSendClicked();
        } else {
            screen = 'connecting';
            await identifyAndProceed(name, role);
        }
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
        console.log('[App] startSender: creating session...');
        const res = await createSession();
        console.log('[App] createSession result:', res.status, res.data);
        if (res.status !== 201) {
            errorMsg = res.data || res.error || t('error');
            screen = 'entry';
            return;
        }

        encryptionKey = generateKey();

        console.log('[App] connecting WebSocket...');
        try {
            await connect();
            console.log('[App] WebSocket connected');
        } catch (e) {
            console.error('[App] WebSocket failed:', e);
            errorMsg = t('error') + ': WebSocket';
            screen = 'entry';
            return;
        }

        on('start_init', (msg) => {
            console.log('[App] start_init received');
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

    function handleComplete({ status, blob }) {
        disconnect();
        completeStatus = status;
        completeBlob = blob || null;
        completeFileName = sessionData?.state?.file?.name || 'download';
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

    function handleBackToEntry() {
        pendingFile = null;
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
            <EntryScreen name={userName} onstart={handleStart} onjoinlink={handleJoinLink} />
            {#if errorMsg}
                <p class="error">{errorMsg}</p>
            {/if}
        {:else if screen === 'file_select'}
            <div class="file-select-screen">
                <h2>{t('selectFile')}</h2>
                <label class="file-picker">
                    <input type="file" onchange={(e) => {
                        const f = e.target.files?.[0];
                        if (f) handleFileSelected(f);
                    }} />
                    <span class="file-picker-label">{pendingFile ? pendingFile.name : t('selectFile')}</span>
                </label>
                <button class="back-btn" onclick={handleBackToEntry}>{t('startOver')}</button>
            </div>
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

    .file-select-screen {
        text-align: center;
        max-width: 400px;
        width: 100%;
        padding: 0 1rem;
    }

    .file-select-screen h2 {
        color: #eee;
        margin-bottom: 1.5rem;
    }

    .file-picker {
        display: block;
        background: #16213e;
        border: 2px dashed #0f3460;
        border-radius: 8px;
        padding: 2rem;
        cursor: pointer;
        transition: border-color 0.2s;
    }

    .file-picker:hover {
        border-color: #e94560;
    }

    .file-picker input {
        display: none;
    }

    .file-picker-label {
        color: #999;
        font-size: 0.95rem;
    }

    .back-btn {
        margin-top: 1rem;
        padding: 0.5rem 1.5rem;
        background: transparent;
        color: #999;
        border: 1px solid #333;
        border-radius: 4px;
        cursor: pointer;
        font-family: inherit;
        font-size: 0.85rem;
    }

    .back-btn:hover {
        color: #eee;
        border-color: #666;
    }
</style>
