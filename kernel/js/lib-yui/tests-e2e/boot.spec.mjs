/***********************************************************************
 *          boot.spec.mjs
 *
 *      Smoke: the test-app boots clean, the default route /dash/ov
 *      renders the Overview card in the centre stage, and there are
 *      no console errors during the load.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


test.describe("boot", () => {

    test("default preset lands on /dash/ov with the Overview card", async ({ page }) => {
        const console_errors = [];
        page.on("console", msg => {
            if(msg.type() === "error") {
                console_errors.push(msg.text());
            }
        });

        await page.goto("/");

        /*  The shell publishes EV_ROUTE_CHANGED right after boot; once
         *  it has, the centre stage holds the Overview card. */
        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card).toBeVisible();
        await expect(card.locator("h1")).toHaveText("Overview");

        /*  The hash mirrors current_route. */
        await expect(page).toHaveURL(/#\/dash\/ov$/);

        /*  The primary nav has its active item highlighted (Dashboard). */
        let active = page.locator('aside[aria-label="primary"] .menu-list a.is-active').first();
        await expect(active).toBeVisible();
        await expect(active.locator(".yui-nav-label")).toHaveText("Dashboard");

        /*  No console errors during boot. */
        expect(console_errors).toEqual([]);
    });

    test("accordion preset boots and the Dashboard branch is auto-expanded", async ({ page }) => {
        await page.goto("/?preset=accordion");

        /*  Accordion lives in the left zone (desktop viewport from the
         *  default Chromium project). */
        let accordion = page.locator('aside[aria-label="tree"]');
        await expect(accordion).toBeVisible();

        /*  The Dashboard heading should be the auto-expanded one (the
         *  /dash/ov route falls under it). */
        let dash_head = accordion.locator('.yui-accordion-head[data-item-id="dash"]');
        await expect(dash_head).toHaveAttribute("aria-expanded", "true");

        /*  And the Overview card lands in the centre stage. */
        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card.locator("h1")).toHaveText("Overview");
    });

});
