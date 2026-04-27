/***********************************************************************
 *          navigation.spec.mjs
 *
 *      Smoke: clicking through every primary item changes the centre-
 *      stage card and the active highlight, and the URL hash mirrors
 *      the active route.  Covers click-driven and hash-driven navigation
 *      both, plus the back/forward buttons.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


/*  Match the test-app's default app_config.json — keep in sync if items
 *  are added or renamed. */
const PRIMARY_ROUTES = [
    { label: "Dashboard", route: "/dash",     /* container; redirects to /dash/ov */ landing: "/dash/ov", title: "Overview" },
    { label: "Reports",   route: "/reports",                                          landing: "/reports/daily", title: "Daily reports" },
    { label: "Settings",  route: "/settings",                                         landing: "/settings",      title: "Settings" },
    { label: "Help",      route: "/help",                                             landing: "/help",          title: "Help (eager)" }
];


async function expect_landed(page, item)
{
    await expect(page).toHaveURL(new RegExp(`#${item.landing.replace(/\//g, "\\/")}$`));
    let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
    await expect(card.locator("h1")).toHaveText(item.title);
}


test.describe("navigation", () => {

    test("clicking each primary item updates the URL hash and the centre stage", async ({ page }) => {
        await page.goto("/");

        for(let item of PRIMARY_ROUTES) {
            let link = page.locator(
                `aside[aria-label="primary"] .menu-list a[data-item-id="${item.route.replace(/^\//, "").split("/")[0]}"]`
            ).first();
            await link.click();
            await expect_landed(page, item);
        }
    });

    test("hash navigation (typed URL) lands on the right card", async ({ page }) => {
        await page.goto("/#/reports/monthly");

        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card.locator("h1")).toHaveText("Monthly reports");
        await expect(page).toHaveURL(/#\/reports\/monthly$/);
    });

    test("browser back / forward replays the route history", async ({ page }) => {
        await page.goto("/");

        /*  Build a 3-step history: /dash/ov → /reports/daily → /settings */
        await page.locator(
            'aside[aria-label="primary"] .menu-list a[data-item-id="reports"]'
        ).first().click();
        await expect(page).toHaveURL(/#\/reports\/daily$/);

        await page.locator(
            'aside[aria-label="primary"] .menu-list a[data-item-id="settings"]'
        ).first().click();
        await expect(page).toHaveURL(/#\/settings$/);

        await page.goBack();
        await expect(page).toHaveURL(/#\/reports\/daily$/);

        await page.goBack();
        await expect(page).toHaveURL(/#\/dash\/ov$/);

        await page.goForward();
        await expect(page).toHaveURL(/#\/reports\/daily$/);
    });

    test("submenu container (/dash) redirects to its first sub-item (/dash/ov)", async ({ page }) => {
        await page.goto("/#/dash");

        await expect(page).toHaveURL(/#\/dash\/ov$/);
        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card.locator("h1")).toHaveText("Overview");
    });

});
