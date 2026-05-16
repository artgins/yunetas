/***********************************************************************
 *          vite-plugin-yuneta-html.js
 *
 *          Vite plugin for Yuneta GUI projects.
 *          Reads config.json and injects:
 *            - <title> from config.title
 *            - <meta> tags from config.metadata
 *            - Content-Security-Policy from config.csp_connect_src
 *
 *          Usage in vite.config.js:
 *            import { yunetaHtmlPlugin } from "@yuneta/lib-yui/vite-plugin-yuneta-html.js";
 *            plugins: [ yunetaHtmlPlugin({ defaultTitle: "My App" }) ]
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import fs from "fs";
import path from "path";

/**
 *  @param {object} [options]
 *  @param {string} [options.defaultTitle]  - Fallback title if config.json is missing
 *  @param {string} [options.configPath]    - Path to config.json (default: ./config.json)
 */
export function yunetaHtmlPlugin(options = {}) {
    const defaultTitle = options.defaultTitle || "Yuneta GUI";

    return {
        name: "yuneta-html-transform",
        transformIndexHtml: {
            order: "pre",
            handler(html) {
                const configPath = options.configPath ||
                    path.resolve(process.cwd(), "config.json");

                if (!fs.existsSync(configPath)) {
                    console.error("⚠️ config.json not found at", configPath);
                    return html;
                }

                const config = JSON.parse(fs.readFileSync(configPath, "utf-8"));

                /*------------------------------------------*
                 *  Title
                 *------------------------------------------*/
                const title = config.title || defaultTitle;

                /*------------------------------------------*
                 *  Metadata
                 *------------------------------------------*/
                let metadataHtml = "";
                if (config.metadata) {
                    for (const [key, value] of Object.entries(config.metadata)) {
                        if (value) {
                            metadataHtml += `  <meta name="${key}" content="${value}">\n`;
                        }
                    }
                }

                /*------------------------------------------*
                 *  Content-Security-Policy
                 *
                 *  Build the CSP <meta> tag from config.
                 *  csp_connect_src is an array of origins.
                 *  Entries starting with "_comment" are
                 *  ignored (documentation hints).
                 *
                 *  ⚠ The emitted policy pins `script-src 'self'` —
                 *  *no* inline <script>, no eval, no `data:` script
                 *  imports, no Function() constructor.  Consumers
                 *  that need to run JS before the bundle (anti-FOUC,
                 *  theme bootstrap, feature detection) must put the
                 *  script in `gui/public/<name>.js` and reference it
                 *  with `<script src="/<name>.js">`.  Same-origin URL
                 *  satisfies 'self'; an inline block does not.
                 *
                 *  See `wattyzer/gui/public/anti-fouc.js` for the
                 *  canonical pre-paint pattern (theme bootstrap from
                 *  localStorage + matchMedia fallback).
                 *
                 *  Style is more permissive ('unsafe-inline'): Bulma
                 *  / view templates need it for inline `style=` and
                 *  injected style blocks.  Don't loosen the script
                 *  side without a strong reason.
                 *------------------------------------------*/
                let cspHtml = "";
                if (config.csp_connect_src) {
                    const origins = config.csp_connect_src
                        .filter(s => !s.startsWith("_comment"));

                    const connectSrc = ["'self'", ...origins].join("\n            ");

                    cspHtml = `<meta http-equiv="Content-Security-Policy"
        content="default-src 'self';
          script-src 'self';
          style-src 'self' 'unsafe-inline';
          connect-src ${connectSrc};
          worker-src 'self' blob:;
          child-src 'self' blob:;
          img-src 'self' data: blob:;
          font-src 'self' data:;">`;
                }

                /*------------------------------------------*
                 *  Apply replacements
                 *------------------------------------------*/
                return html
                    .replace("<title></title>", `<title>${title}</title>`)
                    .replace("<!-- METADATA_PLACEHOLDER -->", metadataHtml)
                    .replace("<!-- CSP_PLACEHOLDER -->", cspHtml);
            }
        }
    };
}
