/***********************************************************************
 *          lifecycle.spec.mjs
 *
 *      Smoke for the per-item lifecycle (`eager` / `keep_alive` /
 *      `lazy_destroy`).  C_TEST_VIEW renders its monotonic
 *      `instance #` in the card body, so we can tell whether the
 *      view was destroyed and rebuilt or just hidden.
 *
 *      Default preset:
 *          /dash/ov     → lifecycle:"keep_alive"   (Overview)
 *          /dash/alerts → lifecycle:"lazy_destroy" (Alerts)
 *          /help        → lifecycle:"eager"        (preinstantiated)
 ***********************************************************************/
import { test, expect } from "@playwright/test";


async function instance_of(page, route_match)
{
    let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
    let txt = await card.locator("p.is-size-7").innerText();
    let m = /instance #(\d+)/.exec(txt);
    if(!m) {
        throw new Error(`No instance # in ${route_match}: '${txt}'`);
    }
    return Number(m[1]);
}


test.describe("lifecycle", () => {

    test("keep_alive: revisiting the same route reuses the instance", async ({ page }) => {
        await page.goto("/");
        let n0 = await instance_of(page, "/dash/ov");

        await page.goto("/#/reports/daily");
        await expect(page).toHaveURL(/#\/reports\/daily$/);

        await page.goto("/#/dash/ov");
        await expect(page).toHaveURL(/#\/dash\/ov$/);
        let n1 = await instance_of(page, "/dash/ov");

        expect(n1).toBe(n0);
    });

    test("lazy_destroy: revisiting the same route mints a new instance", async ({ page }) => {
        await page.goto("/#/dash/alerts");
        let n0 = await instance_of(page, "/dash/alerts");

        await page.goto("/#/dash/ov");
        await expect(page).toHaveURL(/#\/dash\/ov$/);

        await page.goto("/#/dash/alerts");
        await expect(page).toHaveURL(/#\/dash\/alerts$/);
        let n1 = await instance_of(page, "/dash/alerts");

        expect(n1).toBeGreaterThan(n0);
    });

    test("eager: the Help view is preinstantiated before its first visit", async ({ page }) => {
        await page.goto("/");

        /*  At this point the centre stage shows /dash/ov (Overview).
         *  Help is preinstantiated hidden in the same zone.  Find it
         *  via the title attribute of the gobj name embedded in the
         *  paragraph; its instance # exists already. */
        let hidden_help = page.locator(
            '.yui-zone-center .view-card.is-hidden:has(h1:has-text("Help (eager)"))'
        );
        await expect(hidden_help).toHaveCount(1);

        let help_txt = await hidden_help.locator("p.is-size-7").innerText();
        let help_m = /instance #(\d+)/.exec(help_txt);
        expect(help_m).not.toBeNull();
        let preinst_id = Number(help_m[1]);

        /*  Now visit Help.  The instance # must be the same as the
         *  preinstantiated one — the eager view was just unhidden,
         *  not rebuilt. */
        await page.goto("/#/help");
        let n_help = await instance_of(page, "/help");
        expect(n_help).toBe(preinst_id);
    });

});
