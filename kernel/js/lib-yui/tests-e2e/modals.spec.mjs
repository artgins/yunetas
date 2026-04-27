/***********************************************************************
 *          modals.spec.mjs
 *
 *      Smoke for the new modal/notification helpers + the
 *      cross-overlay Escape priority chain (modal over drawer).
 *
 *      Drives the API through the test-app's toolbar buttons
 *      (`Hello` → `EV_SHOW_HELLO` → `yui_shell_show_info`,
 *      `Ask` → `EV_ASK_QUESTION` → `yui_shell_confirm_yesno`),
 *      so we exercise the same path real apps would.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


test.describe("notifications + modals", () => {

    test("Hello toolbar item paints a Bulma .notification on the notification layer", async ({ page }) => {
        await page.goto("/");

        let toast = page.locator(".yui-layer-notification .notification");
        await expect(toast).toHaveCount(0);

        await page.locator('[data-toolbar-item-id="say-hello"]').click();

        await expect(toast).toHaveCount(1);
        await expect(toast).toContainText("Hello from the shell!");
        await expect(toast).toHaveClass(/is-info/);

        /*  Manual dismiss via the .delete button. */
        await toast.locator(".delete").click();
        await expect(toast).toHaveCount(0);
    });

    test("Ask toolbar item opens a yes/no dialog; clicking Yes resolves and shows a follow-up toast", async ({ page }) => {
        await page.goto("/");

        await page.locator('[data-toolbar-item-id="ask-question"]').click();

        let modal = page.locator(".yui-layer-modal .modal.is-active");
        await expect(modal).toBeVisible();
        await expect(modal.locator(".modal-card-title")).toHaveText("A small smoke check");

        /*  Yes button — the primary-styled one. */
        await modal.locator('button[data-modal-button-value="yes"]').click();
        await expect(modal).toHaveCount(0);

        let toast = page.locator(".yui-layer-notification .notification");
        await expect(toast).toContainText("Glad to hear it!");
    });

    test("Escape on an open dialog dismisses it (= clicks the last button)", async ({ page }) => {
        await page.goto("/");

        await page.locator('[data-toolbar-item-id="ask-question"]').click();
        let modal = page.locator(".yui-layer-modal .modal.is-active");
        await expect(modal).toBeVisible();

        await page.keyboard.press("Escape");
        await expect(modal).toHaveCount(0);

        /*  yes/no's last button = "no" → "Noted — check the console.". */
        let toast = page.locator(".yui-layer-notification .notification");
        await expect(toast).toContainText("Noted");
    });

    test("Modal over drawer: first Escape closes the modal, second Escape closes the drawer", async ({ page }) => {
        await page.goto("/");

        /*  1. Open the drawer via the burger. */
        await page.locator('[data-toolbar-item-id="burger"]').click();
        let drawer = page.locator(".yui-drawer");
        await expect(drawer).toHaveClass(/is-active/);

        /*  2. The drawer (z-index 18) occludes the toolbar, so a real
         *  pointer click on the Ask button is blocked.  Use a
         *  synthetic .click() inside the page — Element.click()
         *  dispatches the click event directly on the target without
         *  hit-testing.  Real apps would use the public modal API
         *  (yui_shell_confirm_yesno) instead; this is the e2e shortcut. */
        await page.evaluate(() => {
            let btn = document.querySelector('[data-toolbar-item-id="ask-question"]');
            if(btn) {
                btn.click();
            }
        });

        let modal = page.locator(".yui-layer-modal .modal.is-active");
        await expect(modal).toBeVisible();

        /*  3. First Escape closes the modal — drawer must remain open. */
        await page.keyboard.press("Escape");
        await expect(modal).toHaveCount(0);
        await expect(drawer).toHaveClass(/is-active/);

        /*  4. Second Escape closes the drawer. */
        await page.keyboard.press("Escape");
        await expect(drawer).not.toHaveClass(/is-active/);
    });

});
