/*
 *  Offline test of the doc.yuneta.io service worker.
 *
 *  Run it against the DEPLOYED site after a deploy.sh that touched sw.js:
 *
 *      cd /yuneta/development/projects/wattyzer/gui
 *      node /yuneta/development/yunetas/docs/doc.yuneta.io/pwa/offline-test.mjs
 *
 *  (that directory only for its node_modules -- playwright lives there,
 *  with firefox).
 *
 *  How "offline" is done here, and why not the obvious way:
 *  context.setOffline() is a NO-OP in Playwright's Firefox. With it on,
 *  a page that had never been fetched still loaded from the network, so
 *  every assertion passed while proving nothing. Offline is instead a
 *  second launch of the SAME profile pointed at a dead proxy: nothing
 *  the browser asks for, service-worker fetches included, can leave.
 */
import { firefox } from "/yuneta/development/projects/wattyzer/gui/node_modules/playwright/index.mjs";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const SITE = "https://doc.yuneta.io";
const PROFILE = mkdtempSync(join(tmpdir(), "swtest-"));

function ok(cond, msg) {
    console.log(`${cond? "PASS": "FAIL"}  ${msg}`);
    if(!cond) {
        process.exitCode = 1;
    }
}

async function cached_pages(page) {
    const all = await page.evaluate(async () => {
        const out = [];
        for(const name of await caches.keys()) {
            const c = await caches.open(name);
            for(const r of await c.keys()) {
                out.push(r.url.replace("https://doc.yuneta.io", ""));
            }
        }
        return out;
    });
    return all.filter(u => !u.startsWith("/build"));
}

/*----------------------------------------------------------------*
 *      Phase 1 -- online
 *----------------------------------------------------------------*/
let context = await firefox.launchPersistentContext(PROFILE, {});
let page = await context.newPage();

await page.goto(`${SITE}/`, {waitUntil: "load"});
await page.waitForFunction(async () => {
    const reg = await navigator.serviceWorker.ready;
    return reg.active && reg.active.state === "activated";
}, null, {timeout: 30000});
ok(true, "service worker reaches activated");
ok(await page.evaluate(() => !!navigator.serviceWorker.controller),
    "the first load is already controlled (clients.claim)");

/*  Reload so the start page itself is cached: the first navigation
 *  happened before the worker existed.
 */
await page.reload({waitUntil: "load"});
await page.waitForTimeout(3000);          /*  let it hydrate  */

/*  (a) reached by an in-app click, the way a reader moves  */
await page.click('a[href="/get-started"]');
await page.waitForURL("**/get-started", {timeout: 15000});
const get_started_title = await page.title();

/*  (b) reached by a direct load, the way a deep link arrives  */
await page.goto(`${SITE}/installation`, {waitUntil: "load"});
const installation_title = await page.title();
await page.waitForTimeout(2000);

console.log(`      cached: ${(await cached_pages(page)).join(" | ")}`);
await context.close();

/*----------------------------------------------------------------*
 *      Phase 2 -- same profile, no way out
 *----------------------------------------------------------------*/
context = await firefox.launchPersistentContext(PROFILE, {
    proxy: {server: "http://127.0.0.1:9"},
});
page = await context.newPage();

/*  The installed app opens here.  */
await page.goto(`${SITE}/`, {waitUntil: "load"});
ok((await page.title()).includes("Yuneta"), `offline, the start page renders: "${await page.title()}"`);
await page.waitForTimeout(3000);

/*  In-app click to a page read before: this is how a reader moves
 *  inside the installed app.
 */
await page.click('a[href="/get-started"]').catch(e => console.log("      click:", String(e).split("\n")[0]));
await page.waitForTimeout(3000);
const in_app_url = page.url();
const in_app_text = await page.evaluate(() => document.body.innerText);
ok(in_app_url.endsWith("/get-started") && !in_app_text.includes("not available offline"),
    `offline, an in-app click opens a page read before: ${in_app_url}`);
ok((await page.title()) === get_started_title, `offline, its title is right: "${await page.title()}"`);

/*  Direct load of a page read before (a deep link, or a reload).  */
await page.goto(`${SITE}/installation`, {waitUntil: "load"});
ok((await page.title()) === installation_title,
    `offline, a directly loaded page renders: "${await page.title()}"`);
ok((await page.evaluate(() => document.body.innerText)).includes("yunetas"),
    "offline, that page carries its text");

/*  A page never read.  */
await page.goto(`${SITE}/yuno-treedb`, {waitUntil: "load"});
ok((await page.evaluate(() => document.body.innerText)).includes("not available offline"),
    "offline, a page never read falls back to the offline page");

/*  A page read with a cold load must answer an in-app click too: that
 *  asks for /<slug>?_data=<route>, which was never fetched.
 */
const data_probe = await page.evaluate(async () => {
    try {
        const resp = await fetch("/installation?_data=routes%2F%24");
        const text = await resp.text();
        return {status: resp.status, len: text.length, html: text.startsWith("<!DOCTYPE")};
    } catch(e) {
        return {error: String(e)};
    }
});
ok(data_probe.status === 200 && data_probe.len > 1000,
    `offline, the data url of a cold-loaded page answers: ${JSON.stringify(data_probe)}`);

console.log(`      cached after: ${(await cached_pages(page)).join(" | ")}`);
await context.close();
