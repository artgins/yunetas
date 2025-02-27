import { defineConfig } from "vite";
import fs from "fs";
import path from "path";

export default defineConfig({
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
                const title = config.title || "Test Yuneta";

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
