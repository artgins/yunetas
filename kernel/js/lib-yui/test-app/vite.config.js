import { defineConfig } from "vite";
import { fileURLToPath, URL } from "node:url";

/*
 *  Resolve the bare specifiers `@yuneta/lib-yui` and `@yuneta/gobj-js`
 *  directly to their source (bypassing their dist/ bundles) so the
 *  harness runs without first building the libraries.  Sub-path imports
 *  like "@yuneta/lib-yui/src/c_yui_shell.css" keep going through node_
 *  modules (the packages are linked via `file:..` in package.json), so
 *  the alias MUST match the bare specifier only — hence the anchored
 *  regex form.  A plain string alias would also match the prefix and
 *  rewrite sub-paths into non-existent files.
 */
const libYuiSrc = fileURLToPath(new URL("../index.js", import.meta.url));
const gobjJsSrc = fileURLToPath(new URL("../../gobj-js/src/index.js", import.meta.url));

export default defineConfig({
    resolve: {
        preserveSymlinks: true,
        dedupe: ["@yuneta/gobj-js"],
        alias: [
            { find: /^@yuneta\/lib-yui$/, replacement: libYuiSrc },
            { find: /^@yuneta\/gobj-js$/, replacement: gobjJsSrc }
        ]
    },
    build: {
        sourcemap: true,
        chunkSizeWarningLimit: 6000
    },
    server: {
        port: 5180,
        open: true
    }
});
