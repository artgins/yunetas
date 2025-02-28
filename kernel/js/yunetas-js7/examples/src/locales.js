exports.__yuno__ = __yuno__;
exports.t = function(key, options) {
    return i18next.t(key, options);
};
exports.locales = locales;

let locales = {
    /*
     *  Languages variables are loaded files configured in config.json
     *      "app/locales/en.js",
     *      "app/locales/es.js",
     *      ...
     */
    en: en,
    es: es,
};
