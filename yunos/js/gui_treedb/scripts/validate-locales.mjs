/***********************************************************************
 *          validate-locales.mjs
 *
 *          Enforce the i18n key conventions on every bundled locale:
 *
 *            1. Keys are ASCII (no em-dash, accents, etc.).
 *            2. Keys are lower-case.
 *            3. All locales carry the same key set (symmetric).
 *
 *          Run via `npm run validate-locales` or as a vite prebuild
 *          step.  Exits 1 on any violation so CI fails loudly.
 *
 *          Imports the per-locale modules directly (en.js, es.js)
 *          rather than going through locales.js — the wrapper drags in
 *          @yuneta/gobj-js + i18next which expect a browser global.
 *          Add new locales to the LOCALES list below as they land.
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {en} from "../src/locales/en.js";
import {es} from "../src/locales/es.js";

const LOCALES = {en, es};

const ASCII_RE = /^[\x20-\x7e]+$/;

function fail(msg) {
    process.stderr.write(`validate-locales: ${msg}\n`);
}

function main() {
    const codes = Object.keys(LOCALES);
    let errors = 0;

    /*  Per-locale key checks (ASCII + lower-case). */
    const key_sets = {};
    for(const code of codes) {
        const t = (LOCALES[code] && LOCALES[code].translation) || {};
        const keys = Object.keys(t);
        key_sets[code] = new Set(keys);
        for(const k of keys) {
            if(!ASCII_RE.test(k)) {
                fail(`[${code}] non-ASCII key: ${JSON.stringify(k)}`);
                errors++;
            }
            if(k !== k.toLowerCase()) {
                fail(`[${code}] non-lower-case key: ${JSON.stringify(k)}`);
                errors++;
            }
        }
    }

    /*  Symmetry: every locale must carry the same keys as the first. */
    const ref = codes[0];
    const ref_keys = key_sets[ref];
    for(const code of codes.slice(1)) {
        const cur = key_sets[code];
        for(const k of ref_keys) {
            if(!cur.has(k)) {
                fail(`[${code}] missing key present in [${ref}]: ${JSON.stringify(k)}`);
                errors++;
            }
        }
        for(const k of cur) {
            if(!ref_keys.has(k)) {
                fail(`[${code}] extra key not in [${ref}]: ${JSON.stringify(k)}`);
                errors++;
            }
        }
    }

    if(errors > 0) {
        fail(`${errors} violation(s) — see above`);
        process.exit(1);
    }

    process.stdout.write(
        `validate-locales: OK (${codes.length} locales × ${ref_keys.size} keys)\n`);
}

main();
