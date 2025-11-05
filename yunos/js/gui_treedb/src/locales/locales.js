/***********************************************************************
 *          locales.js
 *
 *          i18n
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    log_error,
    gobj_yuno,
    kw_get_local_storage_value,
    gobj_read_attr,
    kw_set_local_storage_value,
} from "yunetas";

import {Settings as luxonSettings} from "luxon";
import i18next from 'i18next';

import {en} from "./en.js";
import {es} from "./es.js";

/***************************************************************
 *
 ***************************************************************/
function get_locales()
{
    return {
        /*
         *      "./en.js",
         *      "./es.js",
         *      ...
         */
        en: en,
        es: es,
    };
}

/***************************************************************
 *
 ***************************************************************/
function setup_locale()
{
    let locale = kw_get_local_storage_value("locale", "es", true);
    let locales = get_locales();
    if (!locales[locale]) {
        log_error("locale UNKNOWN: " + locale);
        locale = "es";
        kw_set_local_storage_value("locale", locale);
    }

    i18next.init(
        {
            lng: locale, // if you're using a language detector, do not define the lng option
            debug: gobj_read_attr(gobj_yuno(), "trace_i18n"),
            resources: locales
        }
    ).then(function(t) {
        // initialized and ready to go!
    });

    switch (locale) {
        case "en":
        case "es":
            luxonSettings.defaultLocale = locale;
            break;
        default:
            log_error("locale UNKNOWN: " + locale);
            break;
    }
}

export {setup_locale};
