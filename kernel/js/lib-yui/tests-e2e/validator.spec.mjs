/***********************************************************************
 *          validator.spec.mjs
 *
 *      Smoke for `validate_config()` — the system-boundary guard
 *      the shell runs at the top of mt_start.  Boots the test-app
 *      with an intentionally broken preset and checks that:
 *
 *          - the shell paints the "invalid config" banner instead
 *            of producing a half-built layout;
 *          - the console carries the matching log_error lines.
 ***********************************************************************/
import { test, expect } from "@playwright/test";


test("invalid preset surfaces the config banner and logs every error", async ({ page }) => {
    let console_errors = [];
    page.on("console", msg => {
        if(msg.type() === "error") {
            console_errors.push(msg.text());
        }
    });

    await page.goto("/?preset=invalid");

    /*  The shell should NOT have built navs/views — instead, a Bulma
     *  `.notification.is-danger` banner appears in the base layer
     *  carrying the "invalid config" line. */
    let banner = page.locator(".yui-layer-base .notification.is-danger");
    await expect(banner).toBeVisible();
    await expect(banner).toContainText("invalid config");

    /*  Every problem in app_config_invalid.json maps to a log_error
     *  emitted by validate_config(): the unknown zone id 'centre'
     *  and the invalid host 'window.foo'.  At least one of them must
     *  appear in the console. */
    let joined = console_errors.join("\n");
    expect(joined).toMatch(/unknown zone 'centre'/);
    expect(joined).toMatch(/invalid host 'window\.foo'/);
});
