import i18next from 'i18next';
import {en} from "./locales/en.js";
import {es} from "./locales/es.js";

function locales()
{
    return {
        /*
         *      "./locales/en.js",
         *      "./locales/es.js",
         *      ...
         */
        en: en,
        es: es,
    };
}

function t(key, options)
{
    return i18next.t(key, options);
}

export { t, locales };
