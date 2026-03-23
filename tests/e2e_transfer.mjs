#!/usr/bin/env node
// E2E tests for Put-In-Pipe: full sender→receiver transfer flow
// Usage: node tests/e2e_transfer.mjs [baseUrl]

import { firefox } from 'playwright';

const BASE = process.argv[2] || 'http://127.0.0.1:2233';
const CRYPTO_OVERHEAD = 40;
// Server default: 5242880 bytes max chunk. Frontend subtracts 40 for crypto overhead.
// So effective plaintext chunk = 5242880 - 40 = 5242840 bytes.
const EFFECTIVE_CHUNK = 5242880 - CRYPTO_OVERHEAD;

let browser = await firefox.launch({ headless: true });
let passed = 0;
let failed = 0;

async function run(name, fn) {
    console.log(`\n${'='.repeat(60)}\n  TEST: ${name}\n${'='.repeat(60)}`);
    try {
        await fn();
        passed++;
        console.log(`\n  ✅ PASSED: ${name}`);
    } catch (err) {
        failed++;
        console.error(`\n  ❌ FAILED: ${name}\n  ${err.message}`);
    }
}

// Helper: create a sender session and return { senderPage, senderCtx, shareLink }
async function createSender(browser, fileBuffer, fileName) {
    const ctx = await browser.newContext();
    const page = await ctx.newPage();
    page.on('console', m => console.log(`  [sender] ${m.text()}`));

    await page.goto(BASE);
    await page.waitForSelector('.btn-send', { timeout: 10000 });
    await page.click('.btn-send');
    await page.waitForSelector('.file-select-screen', { timeout: 5000 });

    await page.locator('input[type=file]').setInputFiles({
        name: fileName,
        mimeType: 'application/octet-stream',
        buffer: fileBuffer,
    });

    await page.waitForSelector('.share-section input[readonly]', { timeout: 15000 });
    const shareLink = await page.inputValue('.share-section input[readonly]');
    if (!shareLink?.includes('#')) throw new Error('Invalid share link');

    return { page, ctx, shareLink };
}

// Helper: open receiver, wait for complete, validate success
async function receiveFile(browser, shareLink, label = 'receiver') {
    const ctx = await browser.newContext();
    const page = await ctx.newPage();
    page.on('console', m => console.log(`  [${label}] ${m.text()}`));

    await page.goto(shareLink);

    // DuckDuckGo reload flow may happen — wait for complete panel
    await page.waitForSelector('.complete-panel', { timeout: 60000 });

    const icon = await page.textContent('.icon');
    const text = await page.textContent('h2');
    console.log(`  [${label}] status: ${icon.trim()} ${text.trim()}`);

    const ok = icon.includes('✓');
    const btnVisible = await page.locator('.download-btn').isVisible();

    await ctx.close();
    return { ok, btnVisible, text: text.trim() };
}

// ─────────────────────────────────────────────────────────
//  Test 1: Small file (single chunk)
// ─────────────────────────────────────────────────────────
await run('Small file transfer (single chunk)', async () => {
    const content = Buffer.from('Hello from e2e test! ' + Date.now());
    const { page: sender, ctx, shareLink } = await createSender(browser, content, 'small.txt');

    const result = await receiveFile(browser, shareLink);
    if (!result.ok) throw new Error('Transfer failed: ' + result.text);
    if (!result.btnVisible) throw new Error('Download button not visible');

    await ctx.close();
});

// ─────────────────────────────────────────────────────────
//  Test 2: Multi-chunk file (> 1 chunk)
// ─────────────────────────────────────────────────────────
await run('Multi-chunk file transfer', async () => {
    // 3 chunks: 2 full + 1 partial
    const size = EFFECTIVE_CHUNK * 2 + 1000;
    const buf = Buffer.alloc(size);
    for (let i = 0; i < size; i++) buf[i] = i & 0xff;

    const { page: sender, ctx, shareLink } = await createSender(browser, buf, 'multi-chunk.bin');

    const result = await receiveFile(browser, shareLink);
    if (!result.ok) throw new Error('Transfer failed: ' + result.text);
    if (!result.btnVisible) throw new Error('Download button not visible');

    await ctx.close();
});

// ─────────────────────────────────────────────────────────
//  Test 3: Multiple receivers
// ─────────────────────────────────────────────────────────
await run('Multiple receivers', async () => {
    const content = Buffer.from('Multi-receiver test ' + Date.now());
    const { page: sender, ctx, shareLink } = await createSender(browser, content, 'multi-recv.txt');

    // Drop freeze so transfer can start for all
    // Wait for the freeze button to appear (means receivers can join)
    // First receiver joins
    const r1 = receiveFile(browser, shareLink, 'recv1');

    // Wait a bit then second receiver joins
    await new Promise(r => setTimeout(r, 1000));
    const r2 = receiveFile(browser, shareLink, 'recv2');

    const [result1, result2] = await Promise.all([r1, r2]);

    if (!result1.ok) throw new Error('Receiver 1 failed: ' + result1.text);
    if (!result2.ok) throw new Error('Receiver 2 failed: ' + result2.text);

    await ctx.close();
});

// ─────────────────────────────────────────────────────────
//  Test 4: Sender WS disconnect mid-transfer (large file)
// ─────────────────────────────────────────────────────────
await run('Sender WS reconnect mid-transfer', async () => {
    // Large enough for multiple chunks so we can interrupt mid-upload
    const size = EFFECTIVE_CHUNK * 3;
    const buf = Buffer.alloc(size);
    for (let i = 0; i < size; i++) buf[i] = i & 0xff;

    const ctx = await browser.newContext();
    const sender = await ctx.newPage();
    sender.on('console', m => console.log(`  [sender] ${m.text()}`));

    await sender.goto(BASE);
    await sender.waitForSelector('.btn-send', { timeout: 10000 });
    await sender.click('.btn-send');
    await sender.waitForSelector('.file-select-screen', { timeout: 5000 });

    await sender.locator('input[type=file]').setInputFiles({
        name: 'reconnect-test.bin',
        mimeType: 'application/octet-stream',
        buffer: buf,
    });

    await sender.waitForSelector('.share-section input[readonly]', { timeout: 15000 });
    const shareLink = await sender.inputValue('.share-section input[readonly]');

    // Wait for at least 1 chunk to be sent
    await sender.waitForFunction(() => {
        const el = document.querySelector('.buffer-info');
        if (!el) return false;
        const m = el.textContent.match(/(\d+)\//);
        return m && parseInt(m[1]) >= 1;
    }, { timeout: 15000 });
    console.log('  At least 1 chunk sent, dropping network...');

    // Drop sender network to simulate WS disconnect
    await ctx.setOffline(true);
    await new Promise(r => setTimeout(r, 2000));
    console.log('  Restoring network...');
    await ctx.setOffline(false);

    // Wait for upload to resume and complete
    await new Promise(r => setTimeout(r, 3000));

    // Now connect receiver
    const result = await receiveFile(browser, shareLink);
    if (!result.ok) throw new Error('Transfer failed after reconnect: ' + result.text);

    await ctx.close();
});

// ─────────────────────────────────────────────────────────
//  Test 5: Receiver WS disconnect mid-transfer
// ─────────────────────────────────────────────────────────
await run('Receiver WS reconnect mid-transfer', async () => {
    const size = EFFECTIVE_CHUNK * 3;
    const buf = Buffer.alloc(size);
    for (let i = 0; i < size; i++) buf[i] = i & 0xff;

    const { page: sender, ctx: senderCtx, shareLink } = await createSender(browser, buf, 'recv-reconnect.bin');

    const recvCtx = await browser.newContext();
    const receiver = await recvCtx.newPage();
    receiver.on('console', m => console.log(`  [receiver] ${m.text()}`));

    await receiver.goto(shareLink);

    // Wait for receiver to start getting chunks
    await receiver.waitForSelector('.chunk-info', { timeout: 30000 });
    await new Promise(r => setTimeout(r, 1000));
    console.log('  Receiver getting chunks, dropping network...');

    // Drop receiver network
    await recvCtx.setOffline(true);
    await new Promise(r => setTimeout(r, 3000));
    console.log('  Restoring receiver network...');
    await recvCtx.setOffline(false);

    // Wait for completion
    await receiver.waitForSelector('.complete-panel', { timeout: 60000 });
    const icon = await receiver.textContent('.icon');
    const text = await receiver.textContent('h2');
    console.log(`  [receiver] status: ${icon.trim()} ${text.trim()}`);

    if (!icon.includes('✓')) throw new Error('Receiver failed after reconnect: ' + text);

    await senderCtx.close();
    await recvCtx.close();
});

// ─────────────────────────────────────────────────────────
//  Summary
// ─────────────────────────────────────────────────────────
console.log(`\n${'='.repeat(60)}`);
console.log(`  Results: ${passed} passed, ${failed} failed out of ${passed + failed}`);
console.log('='.repeat(60));

if (browser) await browser.close();
process.exitCode = failed > 0 ? 1 : 0;
