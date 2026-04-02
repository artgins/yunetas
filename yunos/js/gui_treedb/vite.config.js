import { defineConfig } from "vite";
import { yunetaHtmlPlugin } from "@yuneta/lib-yui/vite-plugin-yuneta-html.js";

export default defineConfig({
    resolve: {
        preserveSymlinks: true,
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
