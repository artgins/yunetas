/***********************************************************************
 *          i18n.spec.mjs
 *
 *      Smoke for the canonical language switch
 *      (`refresh_language(shell.$container, t)`).
 *
 *      The test-app's toolbar has an ES/EN button that fires
 *      `EV_TOGGLE_LANGUAGE`; the C_TEST_LANG controller flips
 *      between two trivial dictionaries (EN identity / ES map) and
 *      calls `refresh_language` on the shell's container.
 *
 *      What we assert:
 *          - menu labels swap (Dashboard → Panel),
 *          - secondary-nav heading swaps (Dashboard → Panel),
 *          - toolbar button labels swap (Home → Inicio),
 *          - the active item highlight survives the swap (no DOM
 *            rebuild — just `[data-i18n]` text-node retraduction).
 ***********************************************************************/
import { test, expect } from "@playwright/test";


test.describe("live i18n", () => {

    test("ES/EN button flips every translatable label and preserves the active highlight", async ({ page }) => {
        await page.goto("/");
        await expect(page).toHaveURL(/#\/dash\/ov$/);

        /*  Anchors. */
        let primary_dash = page.locator(
            'aside[aria-label="primary"] a[data-item-id="dash"] .yui-nav-label'
        ).first();
        let toolbar_home = page.locator(
            '[data-toolbar-item-id="home"] span[data-i18n="Home"]'
        );
        let secondary_heading = page.locator(
            '[data-nav-zone="right"][aria-label="Dashboard"] .menu-label'
        );

        /*  Initial language: English. */
        await expect(primary_dash).toHaveText("Dashboard");
        await expect(toolbar_home).toHaveText("Home");
        await expect(secondary_heading).toHaveText("Dashboard");

        /*  Snapshot the active highlight before the swap. */
        let active_before = await page.locator(
            'aside[aria-label="primary"] li.is-active a'
        ).first().getAttribute("data-item-id");

        /*  Click ES/EN. */
        await page.locator('[data-toolbar-item-id="lang"]').click();

        /*  Spanish dictionary applied. */
        await expect(primary_dash).toHaveText("Panel");
        await expect(toolbar_home).toHaveText("Inicio");
        await expect(secondary_heading).toHaveText("Panel");

        /*  Active highlight unchanged — refresh_language only touches
         *  `[data-i18n]` text nodes, the `is-active` class on the
         *  parent <li> survives. */
        let active_after = await page.locator(
            'aside[aria-label="primary"] li.is-active a'
        ).first().getAttribute("data-item-id");
        expect(active_after).toBe(active_before);

        /*  Click again — flips back to English. */
        await page.locator('[data-toolbar-item-id="lang"]').click();
        await expect(primary_dash).toHaveText("Dashboard");
        await expect(toolbar_home).toHaveText("Home");
    });

});
