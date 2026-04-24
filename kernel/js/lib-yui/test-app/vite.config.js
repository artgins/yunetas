import { defineConfig } from "vite";
import { fileURLToPath, URL } from "node:url";

/*
 *  Resolve @yuneta/lib-yui and @yuneta/gobj-js directly to their source
 *  (bypass their dist/ bundles) so you can run the test harness without
 *  building them first.  This diverges from how real apps (like
 *  estadodelaire) consume the packages, but it keeps the smoke test
 *  self-contained.
 */
const libYuiSrc = fileURLToPath(new URL("../index.js", import.meta.url));
const gobjJsSrc = fileURLToPath(new URL("../../gobj-js/src/index.js", import.meta.url));

export default defineConfig({
    resolve: {
        preserveSymlinks: true,
        dedupe: ["@yuneta/gobj-js"],
        alias: {
            "@yuneta/lib-yui": libYuiSrc,
            "@yuneta/gobj-js": gobjJsSrc
        }
    },
    server: {
        port: 5180,
        open: true
    }
});
