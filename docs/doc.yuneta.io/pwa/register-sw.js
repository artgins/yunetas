/***********************************************************************
 *          register-sw.js
 *
 *  Register the service worker that gives the documentation offline
 *  reading.  Loaded by <script src> from every page: deploy.sh injects
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
