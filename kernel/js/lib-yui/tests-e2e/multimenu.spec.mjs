/***********************************************************************
 *          multimenu.spec.mjs
 *
 *      Regression for TODO #5 — `instantiate_menus` walks every menu
 *      mounted via a "menu.<id>" host, not just `menu.primary`.
 *
 *      The `multimenu` preset declares two primary-style menus
 *      side-by-side:
 *
 *          menu.primary   (left / bottom)   item id "dash"
 *          menu.admin     (right / bottom-sub) item id "dash"  ← same id!
 *
 *      Each item declares a submenu with its own `render` block.
 *      The shell synthesises the secondary-nav menu_id as
 *      `secondary.<menu_id>.<item.id>` so the two trees never
 *      collide; only the secondary nav owned by the active route's
 *      menu is visible at a time.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


function nav_root_for(page, menu_id)
{
    return page.locator(`aside[aria-label="${menu_id}"]`);
}


test.describe("multiple primary-style menus", () => {

    test("each menu's secondary nav is auto-instantiated and scoped by menu_id", async ({ page }) => {
        await page.goto("/?preset=multimenu");

        /*  The two top-level menus are both rendered as `aside`s with
         *  their own aria-label (the menu id). */
        await expect(nav_root_for(page, "primary").first()).toBeVisible();
        await expect(nav_root_for(page, "admin").first()).toBeVisible();

        /*  Two secondary navs were built (one per menu × item that
         *  declares `submenu.render`).  They are tabs on top-sub. */
        let primary_secondary = page.locator(
            '[data-nav-zone="top-sub"][aria-label="Dashboard"]'
        );
        let admin_secondary = page.locator(
            '[data-nav-zone="top-sub"][aria-label="Admin: Dashboard"]'
        );

        /*  Initial route: /dash/ov → menu.primary owns it →
         *  menu.primary's secondary nav is shown, menu.admin's is
         *  hidden. */
        await expect(primary_secondary).not.toHaveClass(/is-hidden/);
        await expect(admin_secondary).toHaveClass(/is-hidden/);

        /*  Navigate into the admin tree. */
        await page.goto("/?preset=multimenu#/admin/dash/users");

        /*  Centre stage updates. */
        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card.locator("h1")).toHaveText("Admin / Users");

        /*  Now the admin secondary should be visible, primary's
         *  hidden — the visibility flipped strictly by menu_id, not
         *  by the duplicated "dash" item id. */
        await expect(admin_secondary).not.toHaveClass(/is-hidden/);
        await expect(primary_secondary).toHaveClass(/is-hidden/);

        /*  And the secondary's items belong to the right tree. */
        await expect(admin_secondary.locator('a[data-route="/admin/dash/users"]')).toBeVisible();
        await expect(admin_secondary.locator('a[data-route="/admin/dash/billing"]')).toBeVisible();
        await expect(admin_secondary.locator('a[data-route="/dash/ov"]')).toHaveCount(0);
    });

    test("primary navs do NOT cross-highlight items with the same id across menus", async ({ page }) => {
        /*  Both menus declare an item id="dash"; navigating to a
         *  route owned by menu.admin must highlight ONLY menu.admin's
         *  item — not menu.primary's "dash" item.  Without the
         *  menu_id-scoped filter in ac_route_changed, both would
         *  light up. */
        await page.goto("/?preset=multimenu#/admin/dash/users");

        /*  Wait for the centre stage to show the admin card so we
         *  know navigate_to has settled. */
        let card = page.locator(".yui-zone-center .view-card:not(.is-hidden)");
        await expect(card.locator("h1")).toHaveText("Admin / Users");

        /*  Exactly one active item across both primary navs, and it
         *  must be in menu.admin. */
        let admin_active = page.locator(
            'aside[aria-label="admin"] li.is-active a[data-item-id="dash"]'
        );
        let primary_active = page.locator(
            'aside[aria-label="primary"] li.is-active'
        );
        await expect(admin_active).toHaveCount(1);
        await expect(primary_active).toHaveCount(0);

        /*  And the inverse: navigate to a primary-owned route. */
        await page.goto("/?preset=multimenu#/dash/alerts");
        await expect(card.locator("h1")).toHaveText("Alerts");

        let primary_active_2 = page.locator(
            'aside[aria-label="primary"] li.is-active a[data-item-id="dash"]'
        );
        let admin_active_2 = page.locator(
            'aside[aria-label="admin"] li.is-active'
        );
        await expect(primary_active_2).toHaveCount(1);
        await expect(admin_active_2).toHaveCount(0);
    });

});
