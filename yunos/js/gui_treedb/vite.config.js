import { defineConfig } from "vite";
import path from "path";
import { fileURLToPath } from "url";
import { yunetaHtmlPlugin } from "@yuneta/lib-yui/vite-plugin-yuneta-html.js";

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
                find: /^@yuneta\/lib-yui($|\/)/,
                replacement: path.resolve(__dirname, "../../../kernel/js/lib-yui") + "/",
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
        },
        proxy: {
            "/auth": {
                target: "https://localhost:1801",
                changeOrigin: true,
                secure: false      // accept self-signed certs in dev
            }
        }
    },
    test: {
        globals: true,  // Use global `describe` and `test` like Jest
        environment: "node",  // Use Node.js or browser as test environment
        coverage: {
            reporter: ["text", "json", "html"],  // Test coverage output
        },
    },
    plugins: [
        yunetaHtmlPlugin({ defaultTitle: "TreeDB GUI" })
    ]
});
