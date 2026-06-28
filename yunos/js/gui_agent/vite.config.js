/***********************************************************************
 *          vite.config.js
 *
 *          Build config for the Yuneta Agent Console SPA.
 *
 *          Resolution mirrors the in-repo gui_treedb setup:
 *            - @yuneta/gobj-js  -> the submodule SOURCE (src/index.js),
 *              not its built dist/, so the app always tracks the
 *              current kernel source.
 *            - @yuneta/gobj-ui  -> the submodule ROOT, so package
 *              sub-paths (/index.js, /src/*.css) resolve to source.
 *          Both are also declared as `file:` deps in package.json so
 *          npm installs the peer libraries (bulma, i18next).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import { defineConfig } from "vite";
import path from "path";
import { fileURLToPath } from "url";
import { yunetaHtmlPlugin } from "@yuneta/gobj-ui/vite-plugin-yuneta-html.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
    resolve: {
        preserveSymlinks: true,
        alias: [
            {
                find: "@yuneta/gobj-js",
                replacement: path.resolve(__dirname, "../../../kernel/js/gobj-js/src/index.js"),
            },
            {
                find: /^@yuneta\/gobj-ui($|\/)/,
                replacement: path.resolve(__dirname, "../../../kernel/js/gobj-ui") + "/",
            },
        ],
    },
    build: {
        sourcemap: true,
        chunkSizeWarningLimit: 6000
    },
    server: {
        watch: {
            usePolling: true,
            interval: 300
        }
    },
    plugins: [
        yunetaHtmlPlugin({ defaultTitle: "Yuneta Agent Console" })
    ]
});
