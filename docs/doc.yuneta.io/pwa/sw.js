/***********************************************************************
 *          sw.js
 *
 *  Service worker of doc.yuneta.io: offline reading.
 *
 *  What it gives the reader: every page they opened stays readable with
 *  no network, in the browser and in the installed app alike.  What it
 *  does NOT do is download the site in advance.  The whole build is
 *  28 MB (26 MB of theme bundles and 21 MB of page data), so an install
 *  that fetched it all would cost the reader a large download, on a
 *  mobile connection, for pages they may never open.  Everything here is
 *  cached as it is fetched, so the site costs exactly what it costs
 *  today and keeps what went by.
 *
 *  Two caches, two rules:
 *
 *      ASSETS      /build/** -- the mystmd (Remix) bundles and the images
 *                  and fonts they pull.  Every name carries a content
 *                  hash, so a cached copy can never be the wrong one:
 *                  cache first, network only on a miss.
 *
 *      PAGES       the pages themselves.  Each one arrives twice: as the
 *                  server-rendered `/<slug>` document on a cold load,
 *                  and as `/<slug>.json` when the reader clicks a link
 *                  and the app routes on the client.  Both are cached,
 *                  and both go to the network FIRST, because a page that
 *                  was redeployed must not read stale.  The cache is the
 *                  answer when the network fails.
 *
 *  Both cache names carry a version stamped by deploy.sh, and `activate`
 *  deletes every cache that is not of this version.  So a deploy drops
 *  the lot: no mixture of an old page with new bundles can survive it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
"use strict";

const VERSION = "__CACHE_VERSION__";
const ASSETS = `yuneta-docs-assets-${VERSION}`;
const PAGES = `yuneta-docs-pages-${VERSION}`;
const OFFLINE = "/pwa/offline.html";

/*
 *  Bounds, in entries. A page costs its ~200 KB document plus a few KB
 *  of json, so 120 entries is some 60 pages and a cache in the tens of
 *  MB -- enough to hold a long read, small enough that no reader has to
 *  think about it. The oldest go first.
 */
const PAGES_MAX = 120;
const ASSETS_MAX = 400;

/***************************************************************************
 *      Install: only the offline page, which must be there before the
 *      first failure. Everything else is cached as the reader reads.
 ***************************************************************************/
self.addEventListener("install", function(ev) {
    ev.waitUntil((async function() {
        const cache = await caches.open(PAGES);
        /*
         *  cache: "reload" -- take it from the network, never from a
         *  copy the http cache may already hold of an older deploy.
         */
        await cache.add(new Request(OFFLINE, {cache: "reload"}));
        await self.skipWaiting();
    })());
});

/***************************************************************************
 *      Activate: drop every other version and take over the open pages.
 *
 *      claim() + skipWaiting() means a deploy reaches the reader on
 *      their next click, instead of waiting for them to close every tab
 *      of the site -- which, for documentation, can be weeks.
 ***************************************************************************/
self.addEventListener("activate", function(ev) {
    ev.waitUntil((async function() {
        const names = await caches.keys();
        for(const name of names) {
            if(name !== ASSETS && name !== PAGES) {
                await caches.delete(name);
            }
        }
        await self.clients.claim();
    })());
});

/***************************************************************************
 *      Helpers
 ***************************************************************************/
function is_asset(url)
{
    return url.pathname.startsWith("/build/") || url.pathname.startsWith("/pwa/icon");
}

/*
 *  keys() answers in insertion order, so the head of the list is the
 *  oldest. Not awaited by the caller: trimming must never delay the
 *  response the reader is waiting for.
 */
async function trim(cache_name, max)
{
    const cache = await caches.open(cache_name);
    const keys = await cache.keys();
    if(keys.length <= max) {
        return;
    }
    for(const request of keys.slice(0, keys.length - max)) {
        await cache.delete(request);
    }
}

/*
 *  Only a plain 200 can be cached: a 206 makes cache.put() throw, and a
 *  redirect or an error page has no business being served again later.
 */
function is_cacheable(response)
{
    return response && response.status === 200 && response.type === "basic";
}

/*
 *  A page arrives under two urls: `/<slug>` on a cold load, and
 *  `/<slug>?_data=<route>` when the reader clicks a link and the theme
 *  routes on the client. This host is static nginx, which ignores the
 *  query and answers the very same bytes to both -- verified, byte for
 *  byte. So the query is dropped on the way in and on the way out, and
 *  one entry serves both: a page read by a cold load opens on a click,
 *  a page reached by a click survives a reload, and nothing is stored
 *  twice. Any other query is left alone; it may mean something.
 */
function cache_key(request)
{
    const url = new URL(request.url);
    if(url.searchParams.has("_data")) {
        return url.origin + url.pathname;
    }
    return request;
}

async function from_cache_first(request)
{
    const cache = await caches.open(ASSETS);
    const hit = await cache.match(request);
    if(hit) {
        return hit;
    }

    const response = await fetch(request);
    if(is_cacheable(response)) {
        await cache.put(request, response.clone());
        trim(ASSETS, ASSETS_MAX);
    }
    return response;
}

async function from_network_first(request, is_navigation)
{
    const cache = await caches.open(PAGES);
    try {
        const response = await fetch(request);
        if(is_cacheable(response)) {
            await cache.put(cache_key(request), response.clone());
            trim(PAGES, PAGES_MAX);
        }
        return response;

    } catch(e) {
        const hit = await cache.match(cache_key(request));
        if(hit) {
            return hit;
        }
        if(is_navigation) {
            const offline = await cache.match(OFFLINE);
            if(offline) {
                return offline;
            }
        }
        throw e;
    }
}

/***************************************************************************
 *      Fetch
 ***************************************************************************/
self.addEventListener("fetch", function(ev) {
    const request = ev.request;

    if(request.method !== "GET") {
        return;
    }

    /*
     *  A devtools reload issues only-if-cached requests with mode
     *  "same-origin"; answering one from here throws in Chrome.
     */
    if(request.cache === "only-if-cached" && request.mode !== "same-origin") {
        return;
    }

    /*
     *  A range request wants a 206, which is not cacheable and which a
     *  cached 200 would answer wrongly. Leave it to the network.
     */
    if(request.headers.has("range")) {
        return;
    }

    const url = new URL(request.url);
    if(url.origin !== self.location.origin) {
        return;
    }

    /*
     *  The browser owns its own update check for these two, and both are
     *  served no-cache on purpose. Serving them from here would be the
     *  way to make a bad service worker permanent.
     */
    if(url.pathname === "/sw.js" || url.pathname === "/manifest.webmanifest") {
        return;
    }

    if(is_asset(url)) {
        ev.respondWith(from_cache_first(request));
        return;
    }

    ev.respondWith(from_network_first(request, request.mode === "navigate"));
});
