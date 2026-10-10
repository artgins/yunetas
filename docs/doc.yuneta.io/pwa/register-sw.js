/***********************************************************************
 *          register-sw.js
 *
 *  Register the service worker that gives the documentation offline
 *  reading, and link the manifest that makes it installable.  Loaded by <script src> from every page: deploy.sh injects
 *  the tag into the mystmd pages, and the standalone pages carry it in
 *  their own head.
 *
 *  On doc.yuneta.io alone.  yuneta.io, yuneta.com, yuneta.es and
 *  yunetas.com share this docroot, so they serve this file too -- but
 *  their "/" is the landing page, they offer no install, and nginx
 *  answers 404 for /sw.js there.  A service worker is also the hardest
 *  thing on the web to take back: it survives the page that registered
 *  it and answers for the whole origin.  So it goes where it is meant
 *  to be, and nowhere else.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
(function () {
    "use strict";

    if(location.hostname !== "doc.yuneta.io") {
        return;
    }

    /*
     *  The manifest link too, and for the same reason: it is added HERE,
     *  on doc.yuneta.io alone, and not written into the pages.  Written
     *  into them, every page of the four product domains asked for
     *  /manifest.webmanifest and got the 404 nginx gives it there on
     *  purpose -- a 404 per page view, the top of every webstats report
     *  (2026-10-10).  A link added before the browser looks for one is
     *  read the same as a static one.
     */
    if(!document.querySelector('link[rel="manifest"]')) {
        var link = document.createElement("link");
        link.rel = "manifest";
        link.href = "/manifest.webmanifest";
        document.head.appendChild(link);
    }

    if(!("serviceWorker" in navigator)) {
        return;
    }

    /*
     *  After load: registering earlier competes for bandwidth with the
     *  page the reader is waiting for.
     */
    window.addEventListener("load", function () {
        navigator.serviceWorker.register("/sw.js").catch(function (e) {
            console.error("service worker registration failed: " + e);
        });
    });
})();
