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

    test("yui_shell_show_modal: Tab cycles into the .modal-close button (not just .modal-content)", async ({ page }) => {
        await page.goto("/");

        /*  Open a non-blocking modal via the test bridge (window
         *  exposes lib-yui so we can call helpers without a toolbar
         *  entry). */
        await page.evaluate(() => {
            window.__test_modal__ = window.__lib_yui__.yui_shell_show_modal(
                window.__shell__, "Hello modal"
            );
        });

        let modal = page.locator(".yui-layer-modal .modal.is-active");
        await expect(modal).toBeVisible();

        /*  The close button must be in the focus-trap's pool.  We
         *  assert it is reachable via the same selector
         *  activate_focus_trap_on uses internally. */
        let reachable = await page.evaluate(() => {
            let m = document.querySelector(".yui-layer-modal .modal.is-active");
            let close_btn = m && m.querySelector(".modal-close");
            if(!m || !close_btn) {
                return false;
            }
            const SEL = "a[href], button:not([disabled]), input:not([disabled])," +
                        " select:not([disabled]), textarea:not([disabled])," +
                        " [tabindex]:not([tabindex=\"-1\"])";
            let nodes = Array.from(m.querySelectorAll(SEL));
            return nodes.includes(close_btn);
        });
        expect(reachable).toBe(true);

        /*  Sanity: pressing Tab from the close button itself should
         *  wrap to the first focusable inside the modal (focus stays
         *  within $modal, never escapes to elements outside the
         *  modal layer). */
        await page.evaluate(() => {
            let m = document.querySelector(".yui-layer-modal .modal.is-active");
            m.querySelector(".modal-close").focus();
        });
        await page.keyboard.press("Tab");
        let still_inside = await page.evaluate(() => {
            let m = document.querySelector(".yui-layer-modal .modal.is-active");
            return m ? m.contains(document.activeElement) : false;
        });
        expect(still_inside).toBe(true);

        await page.evaluate(() => window.__test_modal__.close());
        await expect(modal).toHaveCount(0);
    });

    test("Modal over drawer: Tab cycles WITHIN the modal only (drawer's trap defers)", async ({ page }) => {
        await page.goto("/");

        /*  1. Open the drawer via the burger. */
        await page.locator('[data-toolbar-item-id="burger"]').click();
        let drawer = page.locator(".yui-drawer");
        await expect(drawer).toHaveClass(/is-active/);

        /*  2. Open a yes/no/cancel dialog programmatically (the
         *  drawer occludes the toolbar so we can't click Ask). */
        await page.evaluate(async () => {
            window.__lib_yui__.yui_shell_confirm_yesnocancel(
                window.__shell__,
                "Save changes?",
                { title: "Confirm" }
            );
        });

        let modal = page.locator(".yui-layer-modal .modal.is-active");
        await expect(modal).toBeVisible();
        let no_btn = modal.locator('button[data-modal-button-value="no"]');
        let cancel_btn = modal.locator('button[data-modal-button-value="cancel"]');

        /*  3. Focus the middle "No" button.  Press Tab — it must
         *  land on Cancel (the next focusable inside the modal),
         *  not snap back to the modal's first focusable.  Without
         *  the LIFO trap-stack fix, the drawer's trap also fires
         *  and forces focus to its first item before the modal
         *  trap runs, which then snaps back to "Yes" every time. */
        await no_btn.focus();
        let active_before = await page.evaluate(() => document.activeElement.getAttribute("data-modal-button-value"));
        expect(active_before).toBe("no");

        await page.keyboard.press("Tab");

        let active_after = await page.evaluate(() => document.activeElement.getAttribute("data-modal-button-value"));
        expect(active_after).toBe("cancel");

        /*  Cleanup: dismiss with Escape (resolves with cancel) and
         *  then close the drawer. */
        await page.keyboard.press("Escape");
        await expect(modal).toHaveCount(0);
        await page.keyboard.press("Escape");
        await expect(drawer).not.toHaveClass(/is-active/);
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
