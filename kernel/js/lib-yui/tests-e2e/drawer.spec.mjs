/***********************************************************************
 *          drawer.spec.mjs
 *
 *      Smoke for the off-canvas drawer + the Escape priority chain.
 *      Today only one overlay class lives in the chain (the drawer);
 *      modal/popup tests will land alongside #4.
 *
 *      The test viewport is the Playwright default (Desktop Chrome /
 *      Desktop Firefox). The drawer is mounted on the overlay layer
 *      and is opened explicitly from the toolbar's burger button —
 *      it is not tied to a breakpoint.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


async function open_drawer_via_burger(page)
{
    await page.locator(
        '.yui-toolbar [data-toolbar-item-id="burger"]'
    ).click();
}


test.describe("drawer + Escape priority chain", () => {

    test("burger opens the drawer; Escape closes it; focus is restored", async ({ page }) => {
        await page.goto("/");

        let drawer = page.locator(".yui-drawer");
        await expect(drawer).not.toHaveClass(/is-active/);

        /*  Click the burger; the drawer becomes active. */
        let burger = page.locator(
            '.yui-toolbar [data-toolbar-item-id="burger"]'
        );
        await burger.click();
        await expect(drawer).toHaveClass(/is-active/);

        /*  Focus moves into the drawer panel (focus-trap activate). */
        let focus_inside_panel = await page.evaluate(() => {
            let panel = document.querySelector(".yui-drawer-panel");
            return panel ? panel.contains(document.activeElement) : false;
        });
        expect(focus_inside_panel).toBe(true);

        /*  Escape closes it. */
        await page.keyboard.press("Escape");
        await expect(drawer).not.toHaveClass(/is-active/);

        /*  Focus is restored to the burger button. */
        let burger_focused = await page.evaluate(() => {
            let b = document.querySelector('[data-toolbar-item-id="burger"]');
            return document.activeElement === b;
        });
        expect(burger_focused).toBe(true);
    });

    test("clicking the backdrop closes the drawer through the canonical close path", async ({ page }) => {
        await page.goto("/");
        await open_drawer_via_burger(page);

        let drawer = page.locator(".yui-drawer");
        await expect(drawer).toHaveClass(/is-active/);

        /*  Click far enough to the right that we hit the backdrop,
         *  not the 18rem-wide panel that sits stacked on top of it.
         *  page.mouse.click() takes absolute viewport coordinates;
         *  the default Playwright viewport is 1280×720. */
        await page.mouse.click(900, 400);
        await expect(drawer).not.toHaveClass(/is-active/);

        /*  Stack popped: a follow-up Escape must be a no-op. */
        await page.keyboard.press("Escape");
        await expect(drawer).not.toHaveClass(/is-active/);
    });

    test("Escape with nothing open is a no-op (URL hash unchanged)", async ({ page }) => {
        await page.goto("/");
        /*  Wait for the shell's initial navigate_to to settle the
         *  hash (Firefox can return from goto() before the
         *  mt_start->history.replaceState() fires). */
        await expect(page).toHaveURL(/#\/dash\/ov$/);
        let url_before = page.url();
        await page.keyboard.press("Escape");
        await page.keyboard.press("Escape");
        expect(page.url()).toBe(url_before);
    });

});
