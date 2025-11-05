import { defineConfig } from "vite";
import fs from "fs";
import path from "path";

export default defineConfig({
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
    test: {
        globals: true,  // Use global `describe` and `test` like Jest
        environment: "node",  // Use Node.js or browser as test environment
        coverage: {
            reporter: ["text", "json", "html"],  // Test coverage output
        },
    },
    plugins: [
        {
            name: "html-transform",
            transformIndexHtml(html) {
                const configPath = path.resolve(__dirname, "config.json");
                if (!fs.existsSync(configPath)) {
                    console.error("⚠️ config.json not found");
                    return html;
                }

                const config = JSON.parse(fs.readFileSync(configPath, "utf-8"));
                const title = config.title || "TreeDB GUI";

                let metadataHtml = "";
                for (const [key, value] of Object.entries(config.metadata)) {
                    if (value) {
                        metadataHtml += `  <meta name="${key}" content="${value}">\n`;
                    }
                }

                return html
                    .replace("<title></title>", `<title>${title}</title>`)
                    .replace("<!-- METADATA_PLACEHOLDER -->", metadataHtml);
            }
        }
    ]
});
