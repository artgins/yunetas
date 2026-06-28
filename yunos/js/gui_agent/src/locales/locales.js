/***********************************************************************
 *          locales.js
 *
 *          i18n bootstrap (i18next). `setup_locale()` reads the user's
 *          chosen language from localStorage (default: "es"), initialises
 *          i18next with all bundled resource bundles, and exposes a tiny
 *          API to switch language at runtime.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    log_error,
    gobj_yuno,
    gobj_read_attr,
    kw_get_local_storage_value,
    kw_set_local_storage_value,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {en} from "./en.js";
import {es} from "./es.js";


const DEFAULT_LOCALE = "es";

function get_locales()
{
    return {
        en: en,
        es: es,
    };
}

function setup_locale()
{
    let locale = kw_get_local_storage_value("locale", DEFAULT_LOCALE, true);
    let locales = get_locales();
    if(!locales[locale]) {
        log_error(`locale UNKNOWN: ${locale}`);
        locale = DEFAULT_LOCALE;
        kw_set_local_storage_value("locale", locale);
    }

    i18next.init({
        lng: locale,
        debug: gobj_read_attr(gobj_yuno(), "trace_i18n"),
        resources: locales,
        initImmediate: false,
    });

    return locale;
}

function current_locale()
{
    return i18next.language || DEFAULT_LOCALE;
}

function switch_locale(locale)
{
    let locales = get_locales();
    if(!locales[locale]) {
        log_error(`switch_locale: unknown '${locale}'`);
        return current_locale();
    }
    i18next.changeLanguage(locale);
    kw_set_local_storage_value("locale", locale);
    return locale;
}

export {setup_locale, switch_locale, current_locale, get_locales};
