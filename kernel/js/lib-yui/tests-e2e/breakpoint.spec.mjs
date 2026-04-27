/***********************************************************************
 *          breakpoint.spec.mjs
 *
 *      Smoke for the responsive zone visibility driven by Bulma
 *      breakpoints.
 *
 *      Default preset (see app_config.json):
 *          left   → menu.primary,  show_on >= desktop  (≥ 1024 px)
 *          bottom → menu.primary,  show_on < desktop   (< 1024 px)
 *          top    → toolbar,       show_on >= tablet   (≥ 769 px)
 *
 *      The shell hides zones with custom classes
 *      `.yui-hidden-mobile` / `.yui-hidden-tablet-only` / etc., each
 *      backed by a `@media` rule in c_yui_shell.css.  We assert
 *      visibility through the layout — the actual presence of the
 *      computed style is a CSS concern of the browser.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


test.describe("breakpoint swap", () => {

    test("desktop (1280×720): primary menu lives in `left`, bottom is hidden", async ({ page }) => {
        await page.setViewportSize({ width: 1280, height: 720 });
        await page.goto("/");

        let left   = page.locator('.yui-zone-left   aside[aria-label="primary"]');
        let bottom = page.locator('.yui-zone-bottom .yui-nav-iconbar');

        await expect(left).toBeVisible();
        await expect(bottom).toBeHidden();
    });

    test("mobile (480×800): primary menu lives in `bottom`, left is hidden", async ({ page }) => {
        await page.setViewportSize({ width: 480, height: 800 });
        await page.goto("/");

        let left   = page.locator('.yui-zone-left   aside[aria-label="primary"]');
        let bottom = page.locator('.yui-zone-bottom .yui-nav-iconbar');

        await expect(bottom).toBeVisible();
        await expect(left).toBeHidden();
    });

    test("tablet (800×600): the toolbar appears, but primary stays at `bottom`", async ({ page }) => {
        await page.setViewportSize({ width: 800, height: 600 });
        await page.goto("/");

        let toolbar = page.locator(".yui-zone-top .yui-toolbar");
        let bottom  = page.locator('.yui-zone-bottom .yui-nav-iconbar');
        let left    = page.locator('.yui-zone-left   aside[aria-label="primary"]');

        /*  769 ≤ width < 1024 → tablet: toolbar visible, primary at
         *  bottom (icon-bar), left hidden. */
        await expect(toolbar).toBeVisible();
        await expect(bottom).toBeVisible();
        await expect(left).toBeHidden();
    });

});
