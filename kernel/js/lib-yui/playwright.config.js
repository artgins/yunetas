/***********************************************************************
 *          playwright.config.js
 *
 *      End-to-end smoke harness for C_YUI_SHELL + C_YUI_NAV.
 *      Boots the test-app via `vite preview` against its built bundle
 *      (deterministic — no HMR, no source watching) and runs the specs
 *      under `tests-e2e/` against chromium and firefox.
 *
 *      Run locally:
 *          npm run test:e2e           # headless, all three browsers
 *          npm run test:e2e:ui        # Playwright UI mode
 *
 *      The first run downloads the browser binaries:
 *          npx playwright install chromium firefox webkit
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import { defineConfig, devices } from "@playwright/test";

const E2E_PORT = 5181;
const E2E_BASE = `http://localhost:${E2E_PORT}/`;

export default defineConfig({
    testDir: "./tests-e2e",
    fullyParallel: true,
    forbidOnly: !!process.env.CI,
    retries: process.env.CI ? 2 : 0,
    workers: process.env.CI ? 1 : undefined,
    reporter: process.env.CI ? "github" : "list",

    use: {
        baseURL: E2E_BASE,
        trace: "on-first-retry"
    },

    projects: [
        {
            name: "chromium",
            use: { ...devices["Desktop Chrome"] }
        },
        {
            name: "firefox",
            use: { ...devices["Desktop Firefox"] }
        },
        {
            name: "webkit",
            use: { ...devices["Desktop Safari"] }
        }
    ],

    /*  Build the test-app once and serve it via vite preview. The dev
     *  server (port 5180) is intentionally NOT used — preview is more
     *  deterministic and matches what users actually ship. */
    webServer: {
        command: `cd test-app && npm run build && npm run preview -- --port ${E2E_PORT} --strictPort`,
        url: E2E_BASE,
        timeout: 120_000,
        reuseExistingServer: !process.env.CI
    }
});
