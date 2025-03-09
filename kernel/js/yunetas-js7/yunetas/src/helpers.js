/*********************************************************************************
 *      Helper functions
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/
import {
    GObj,
    gobj_short_name,
} from "./gobj.js";

const kw_flag_t = Object.freeze({
    KW_REQUIRED     : 0x0001,   // Log error message if not exist.
    KW_CREATE       : 0x0002,   // Create if not exist
    KW_WILD_NUMBER  : 0x0004,   // For numbers work with real/int/bool/string without error logging
    KW_EXTRACT      : 0x0008,   // Extract (delete) the key on read from dictionary.
    KW_BACKWARD     : 0x0010,   // Search backward in lists or arrays
    KW_VERBOSE      : 0x0020,   //
    KW_LOWER        : 0x0040,   //
    KW_RECURSIVE    : 0x0080,   //
});


/************************************************************
 *          log function
 ************************************************************/

/********************************************
 *  Log functions
 ********************************************/
let f_error = window.console.error;
let f_warning = window.console.warn;
let f_info = window.console.info;
let f_debug = window.console.debug;

function set_log_functions(f_error_, f_warning_, f_info_, f_debug_)
{
    if(f_error_) {
        f_error = f_error_;
    }
    if(f_warning_) {
        f_warning = f_warning_;
    }
    if(f_info_) {
        f_info = f_info_;
    }
    if(f_debug_) {
        f_debug = f_debug_;
    }
}

function log_error(msg)
{
    try {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
    } catch (e) {
        msg = "???";
    }

    let hora = current_timestamp();

    if(f_error) {
        f_error("%c" + hora + " ERROR: " + String(msg), "color:red");
    }
}

function log_warning(msg)
{
    try {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
    } catch (e) {
        msg = "???";
    }

    let hora = current_timestamp();
    if(f_warning) {
        f_warning("%c" + hora + " WARNING: " + String(msg), "color:yellow");
    }
}

function log_info(msg)
{
    try {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
    } catch (e) {
        msg = "???";
    }

    let hora = current_timestamp();
    if(f_warning) {
        f_warning("%c" + hora + " INFO: " + String(msg), "color:cyan");
    }
}

function log_debug(msg)
{
    try {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
    } catch (e) {
        msg = "???";
    }

    let hora = current_timestamp();
    if(f_debug) {
        f_debug("%c" + hora + " DEBUG: " + String(msg), "color:silver");
    }
}

function trace_msg(msg)
{
    try {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
    } catch (e) {
        msg = "???";
    }

    let hora = current_timestamp();
    if(f_debug) {
        f_debug("%c" + hora + " MSG: " + String(msg), "color:cyan");
    }
}

function trace_json(jn)
{
    if(window) {
        window.console.dir(jn);
    }
}

/************************************************************
 *  Duplicate an object (new references)
 *  using the modern structuredClone
 ************************************************************/
function json_deep_copy(obj) // old __duplicate__,duplicate_objects
{
    return structuredClone(obj); // Deep copy of a single object
}

/************************************************************
 *  Duplicate objects (new references) in a new object
 *  using the modern structuredClone
 ************************************************************/
function duplicate_objects(...sourceObjects)
{
    const result = {};
    for (const obj of sourceObjects) {
        const clonedObj = structuredClone(obj); // Deep copy using structuredClone
        Object.assign(result, clonedObj); // Merge into the result object
    }
    return result;
}

/************************************************************
 *
 ************************************************************/
function json_is_identical(json1, json2)
{
    if (json1 === json2) {
        return true;  // If both references are the same, they are identical
    }

    if (typeof json1 !== "object" || typeof json2 !== "object" || json1 === null || json2 === null) {
        return false; // If not objects or one is null, they are not identical
    }

    const keys1 = Object.keys(json1);
    const keys2 = Object.keys(json2);

    if (keys1.length !== keys2.length) {
        return false; // Different number of keys → not identical
    }

    for (const key of keys1) {
        if (!json2.hasOwnProperty(key)) {
            return false; // Key missing in json2 → not identical
        }
        if (!json_is_identical(json1[key], json2[key])) {
            return false; // Recursively check nested values
        }
    }

    return true;
}

/************************************************************
 *  Extend a dict with another dict (NOT recursive),
 *  adding new keys and overwriting existing keys.
 ************************************************************/
function json_object_update(destination, source) // old __extend_dict__
{
    if(!source) {
        return destination;
    }
    for (let property of Object.keys(source)) {
        if (source.hasOwnProperty(property)) {
            destination[property] = source[property];
        }
    }
    return destination;
}

/************************************************************
 *  Update a dict with another dict:
 *  ONLY existing items!! (NOT recursive)
 ************************************************************/
function json_object_update_existing(destination, source) // old __update_dict__
{
    if(!source) {
        return destination;
    }
    for (let property in source) {
        if(source.hasOwnProperty(property) && destination.hasOwnProperty(property)) {
            destination[property] = source[property];
        }
    }
    return destination;
}

/************************************************************
 *  Update a dict with another dict:
 *  ONLY missing items!! (NOT recursive)
 ************************************************************/
function json_object_update_missing(destination, source)
{
    if(!source) {
        return destination;
    }
    for (let property in source) {
        if(source.hasOwnProperty(property) && !destination.hasOwnProperty(property)) {
            destination[property] = source[property];
        }
    }
    return destination;
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_object_get(o, key)
{
    if (o.hasOwnProperty(key)) {
        return o[key];
    }
    return undefined;
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_object_del(o, k)
{
    delete o[k];
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_object_set_new(o, k, v)
{
    o[k] = v;
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_array_append(a, v)
{
    a.push(v);
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_array_append_new(a, v)
{
    a.push(v);
}

/************************************************************
 *  Simulate jansson function
 ************************************************************/
function json_array_remove(a, idx)
{
    a.splice(idx, 1);
}

/************************************************************
 *  Extend array
 ************************************************************/
function json_array_extend(destination, source)  // old  __extend_list__
{
    if(!source) {
        return destination;
    }
    Array.prototype.push.apply(destination, source);
    return destination;
}

/************************************************************
 *
 ************************************************************/
function json_object_size(a)
{
    if(is_object(a)) {
        return Object.keys(a).length;
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function json_array_size(a)
{
    if(is_array(a)) {
        return a.length;
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function json_size(a)
{
    if(is_object(a)) {
        return Object.keys(a).length;
    } else if(is_array(a)) {
        return a.length;
    } else if(is_string(a)) {
        return 0;
    } else {
        return 0;
    }
}

/************************************************************
 *  Return if a value is an object (excluding arrays & null)
 ************************************************************/
function is_object(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}

/************************************************************
 *  Return if a value is an array
 ************************************************************/
function is_array(value) {
    return Array.isArray(value);
}

/************************************************************
 *  Return if a value is a string
 ************************************************************/
function is_string(value) {
    return typeof value === "string";
}

/************************************************************
 *  Return if a value is a number (excluding NaN & Infinity)
 ************************************************************/
function is_number(value) {
    return typeof value === "number" && Number.isFinite(value);
}

/************************************************************
 *  Return if a value is a boolean
 ************************************************************/
function is_boolean(value) {
    return typeof value === "boolean";
}

/************************************************************
 *  Return if a value is null or undefined
 ************************************************************/
function is_null(value) {
    return value == null;  // Covers both `null` and `undefined`
}

/************************************************************
 *  Check if a string is a number
 *  The function correctly identifies both integer and floating-point numbers.
 *  It handles special cases like scientific notation (e.g., "1.2e3"),
 *  positive/negative numbers, and fractional numbers without leading digits.
 *  It returns false for non-numeric strings, empty strings,
 *  and strings with only whitespace.
 ************************************************************/
function is_pure_number(str)
{
    if(is_number(str)) {
        return true;
    }
    if(!is_string(str)) {
        return false;
    }

    // Convert the string to a number
    const num = Number(str);

    // Check if the result is NaN (Not-a-Number)
    if (isNaN(num)) {
        return false; // The string is not a number
    }

    // Optionally, you can check if the input is exactly equal to the parsed number
    // This ensures that the input is a valid number string
    return str.trim() !== '' && !isNaN(num);
}

/************************************************************
 *  Return if a value is a Date object
 ************************************************************/
function is_date(value) {
    return value instanceof Date && !isNaN(value);
}

/************************************************************
 *  Return if a value is a function
 ************************************************************/
function is_function(value) {
    return typeof value === "function";
}

/************************************************************
 *  Return if a value is an instance of GObj
 ************************************************************/
function is_gobj(value) {
    return value instanceof GObj;
}

/************************************************************
 *  Return if a value is an empty string or not a string
 ************************************************************/
function empty_string(value) {
    return !is_string(value) || value.length === 0;
}

/************************************************************
 *      Like C functions
 ************************************************************/
function strncmp(str1, str2, lgth)
{
    // Binary safe string comparison
    //
    // version: 909.322
    // discuss at: http://phpjs.org/functions/strncmp
    // +      original by: Waldo Malqui Silva
    // +         input by: Steve Hilder
    // +      improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +       revised by: gorthaur
    // + reimplemented by: Brett Zamir (http://brett-zamir.me)
    // *     example 1: strncmp("aaa", "aab", 2);
    // *     returns 1: 0
    // *     example 2: strncmp("aaa", "aab", 3 );
    // *     returns 2: -1
    var s1 = (str1+"").substring(0, lgth);
    var s2 = (str2+"").substring(0, lgth);

    return ( ( s1 === s2 ) ? 0 : ( ( s1 > s2 ) ? 1 : -1 ) );
}

/************************************************************
 *
 ************************************************************/
function strcmp(str1, str2)
{
    // http://kevin.vanzonneveld.net
    // +   original by: Waldo Malqui Silva
    // +      input by: Steve Hilder
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +    revised by: gorthaur
    // *     example 1: strcmp( "waldo", "owald" );
    // *     returns 1: 1
    // *     example 2: strcmp( "owald", "waldo" );
    // *     returns 2: -1

    return ( ( str1 === str2 ) ? 0 : ( ( str1 > str2 ) ? 1 : -1 ) );
}

/************************************************************
 *
 ************************************************************/
function strcasecmp(str1, str2)
{
    // http://kevin.vanzonneveld.net
    // +   original by: Waldo Malqui Silva
    // +      input by: Steve Hilder
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +    revised by: gorthaur
    // *     example 1: strcmp( "waldo", "owald" );
    // *     returns 1: 1
    // *     example 2: strcmp( "owald", "waldo" );
    // *     returns 2: -1
    let s1 = str1.toLowerCase();
    let s2 = str2.toLowerCase();

    return ( ( s1 === s2 ) ? 0 : ( ( s1 > s2 ) ? 1 : -1 ) );
}

/***************************************************************************
    Only compare str/int/real/bool items
    Complex types are done as matched
    Return lower, iqual, higher (-1, 0, 1), like strcmp
***************************************************************************/
function cmp_two_simple_json(jn_var1, jn_var2)
{
    /*
     *  Discard complex types, done as matched
     */
    if(is_object(jn_var1) ||
            is_object(jn_var2) ||
            is_array(jn_var1) ||
            is_array(jn_var2)) {
        return 0;
    }

    /*
     *  First try number
     */
    if(is_number(jn_var1) || is_number(jn_var2)) {
        let val1 = Number(jn_var1);
        let val2 = Number(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try boolean
     */
    if(is_boolean(jn_var1) || is_boolean(jn_var2)) {
        let val1 = Number(jn_var1);
        let val2 = Number(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try string
     */
    let val1 = String(jn_var1);
    let val2 = String(jn_var2);
    return strcmp(val1, val2);
}

/************************************************************
 *
 ************************************************************/
function strstr(haystack, needle, bool)
{
    // Finds first occurrence of a string within another
    //
    // version: 1103.1210
    // discuss at: http://phpjs.org/functions/strstr
    // +   original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +   bugfixed by: Onno Marsman
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // *     example 1: strstr(‘Kevin van Zonneveld’, ‘van’);
    // *     returns 1: ‘van Zonneveld’
    // *     example 2: strstr(‘Kevin van Zonneveld’, ‘van’, true);
    // *     returns 2: ‘Kevin ‘
    // *     example 3: strstr(‘name@example.com’, ‘@’);
    // *     returns 3: ‘@example.com’
    // *     example 4: strstr(‘name@example.com’, ‘@’, true);
    // *     returns 4: ‘name’
    var pos = 0;

    haystack += "";
    pos = haystack.indexOf(needle);
    if (pos === -1) {
        return false;
    } else {
        if (bool) {
            return haystack.substring(0, pos);
        } else {
            return haystack.slice(pos);
        }
    }
}

/************************************************************
 *
 ************************************************************/
function _kw_match_simple(kw, jn_filter, level)
{
    let matched = false;

    level++;

    if(is_array(jn_filter)) {
        // Empty array evaluate as false, until a match condition occurs.
        matched = false;
        for(let idx = 0; idx < jn_filter.length; idx++) {
            let jn_filter_value = jn_filter[idx];
            matched = _kw_match_simple(
                kw,                 // not owned
                jn_filter_value,    // owned
                level
            );
            if(matched) {
                break;
            }
        }

    } else if(is_object(jn_filter)) {
        if(json_object_size(jn_filter)===0) {
            // Empty object evaluate as false.
            matched = false;
        } else {
            // Not Empty object evaluate as true, until a NOT match condition occurs.
            matched = true;
        }

        for(const filter_path of Object.keys(jn_filter)) {
            let jn_filter_value = jn_filter[filter_path];
            /*
             *  Variable compleja, recursivo
             */
            if(is_array(jn_filter_value) || is_object(jn_filter_value)) {
                matched = _kw_match_simple(
                    kw,
                    jn_filter_value,
                    level
                );
                break;
            }

            /*
             *  Variable sencilla
             */
            /*
             * TODO get the name and op.
             */
            let path = filter_path; // TODO
            let op = "__equal__";

            /*
             *  Get the record value, firstly by path else by name
             */
            let jn_record_value;
            // Firstly try the key as pointers
            jn_record_value = kw_get_dict_value(kw, path, 0, 0);
            if(!jn_record_value) {
                // Secondly try the key with points (.) as full key
                jn_record_value = kw[path];
            }
            if(!jn_record_value) {
                matched = false;
                break;
            }

            /*
             *  Do simple operation
             */
            if(op === "__equal__") { // TODO __equal__ by default
                let cmp = cmp_two_simple_json(jn_record_value, jn_filter_value);
                if(cmp!==0) {
                    matched = false;
                    break;
                }
            } else {
                // TODO op: __lower__ __higher__ __re__ __equal__
            }
        }
    }

    return matched;
}

/***************************************************************************
 *  Metadata key (variable) has a prefix of 2 underscore
 ***************************************************************************/
function is_metadata_key(key)
{
    if(!is_string(key)) {
        return false;
    }
    let i;
    for(i = 0; i < key.length; i++) {
        if (key[i] !== '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i === 2);
}

/***************************************************************************
 *  Private key (variable) has a prefix of 1 underscore
 ***************************************************************************/
function is_private_key(key)
{
    if(!is_string(key)) {
        return false;
    }
    let i;
    for(i = 0; i < key.length; i++) {
        if (key[i] !== '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i === 1);
}

/************************************************************
 * Remove from `kw1` all keys in `kw2`.
 * `kw2` can be a string, object, or array.
 ************************************************************/
function kw_pop(kw1, kw2)
{
    if (typeof kw2 === "object" && !Array.isArray(kw2)) {
        // If kw2 is an object, remove its keys from kw1
        for (const key of Object.keys(kw2)) {
            delete kw1[key];
        }
    } else if (Array.isArray(kw2)) {
        // If kw2 is an array, recursively remove each element from kw1
        for (const item of kw2) {
            kw_pop(kw1, item);
        }
    } else if (typeof kw2 === "string") {
        // If kw2 is a string, remove it as a key from kw1
        delete kw1[kw2];
    }
}

/************************************************************
 *
 ************************************************************/
function kw_has_key(kw, key)
{
    if(key in kw) {
        if (kw.hasOwnProperty(key)) {
            return true;
        }
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function kw_find_path(gobj, kw, path, verbose)
{
    if(!is_object(kw)) {
        // silence
        if(verbose) {
            log_error("kw must be list or dict");
        }
        return 0;
    }
    if(!is_string(path)) {
        if(verbose) {
            log_error("path must be a string: " + String(path));
        }
        return 0;
    }
    let ss = path.split("`");
    if(ss.length<=1) {
        return kw[path];
    }
    let len = ss.length;
    for(let i=0; i<len; i++) {
        let key = ss[i];
        kw = kw[key];
        if(kw === undefined) {
            if(verbose) {
                log_error(`path not found: ${String(path)}`);
            }
            return undefined;
        }
    }
    return kw;
}

/************************************************************
 *
 ************************************************************/
function kw_delete(gobj, kw, path)
{
    // TODO use delimiter `
    json_object_del(kw, path);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function kw_get_bool(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return Boolean(default_value);
    }
    let b = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(b === undefined) {
        if(create) {
            let v = Boolean(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return Boolean(default_value);
    }
    if(extract) {
        kw_delete(gobj, kw, path);
    }
    return Boolean(b);
}

/************************************************************
 *
 ************************************************************/
function kw_get_int(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return parseInt(default_value);
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = parseInt(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return parseInt(default_value);
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    if(!is_number(v)) {
        if(required) {
            log_error(`path value MUST BE a number: ${path}`);
            trace_msg(kw);
        }
        return parseInt(default_value);
    }

    return parseInt(v);
}

/************************************************************
 *
 ************************************************************/
function kw_get_real(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return Number(default_value);
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = Number(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return Number(default_value);
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    if(!is_number(v)) {
        if(required) {
            log_error(`path value MUST BE a number: ${path}`);
            trace_msg(kw);
        }
        return Number(default_value);
    }

    return Number(v);
}

/************************************************************
 *
 ************************************************************/
function kw_get_str(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return String(default_value);
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = String(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return String(default_value);
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    if(!is_string(v)) {
        if(required) {
            log_error(`${gobj_short_name(gobj)}: path value MUST BE a string: ${path}`);
            trace_msg(kw);
        }
        return String(default_value);
    }

    return String(v);
}

/************************************************************
 *
 ************************************************************/
function kw_get_pointer(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return default_value;
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = default_value;
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return default_value;
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    return v;
}

/************************************************************
 *
 ************************************************************/
function kw_get_dict(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return Object(default_value);
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = Object(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return Object(default_value);
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    if(!is_object(v)) {
        if(required) {
            log_error(`path value MUST BE a dict: ${path}`);
            trace_msg(kw);
        }
        return Object(default_value);
    }

    return Object(v);
}

/************************************************************
 *
 ************************************************************/
function kw_get_dict_value(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return default_value;
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = default_value;
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return default_value;
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    return v;
}

/************************************************************
 *
 ************************************************************/
function kw_get_list(gobj, kw, path, default_value, flag)
{
    if(kw !== Object(kw)) {
        return Array(default_value);
    }
    let v = kw_find_path(gobj, kw, path, false);

    const required = flag && (flag & kw_flag_t.KW_REQUIRED);
    const create = flag && (flag & kw_flag_t.KW_CREATE);
    const extract = flag && (flag & kw_flag_t.KW_EXTRACT);

    if(v === undefined) {
        if(create) {
            let v = Array(default_value);
            kw_set_dict_value(gobj, kw, path, v);
            return v;

        } else if(required) {
            log_error(`path not found: '${path}'`);
            trace_json(kw);
        }
        return Array(default_value);
    }

    if(extract) {
        kw_delete(gobj, kw, path);
    }

    if(!is_array(v)) {
        if(required) {
            log_error(`path value MUST BE an array: ${path}`);
            trace_msg(kw);
        }
        return Array(default_value);
    }

    return Array(v);
}

/************************************************************
 *
 ************************************************************/
function kw_set_dict_value(gobj, kw, path, value)
{
    if(!is_object(kw)) {
        log_error("kw is not an object");
        return -1;
    }

    let ss = path.split("`");
    if(ss.length<=1) {
        kw[path] = value;
        return 0;
    }

    let len = ss.length;
    for(let i=0; i<len; i++) {
        let key = ss[i];
        if(i === len-1) {
            /* last segment */
            kw[key] = value;

        } else {
            let seg = kw[key];
            if(seg === undefined) {
                kw[key] = {};
            }
            kw = kw[key];
        }
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function kw_set_subdict_value(gobj, kw, path, key, value)
{
    if(!is_object(kw)) {
        log_error("kw is not an object");
        return -1;
    }

    let subdict = kw_get_dict(kw, path, {}, true, true);
    if(!is_object(subdict)) {
        log_error("subdict is not an object");
        return -1;
    }
    subdict[key] = value;

    return 0;
}

/************************************************************
 *
 ************************************************************/
function kw_match_simple(kw, jn_filter)
{
    if(!jn_filter) {
     // Si no hay filtro pasan todos.
       return true;
    }
    if(is_object(jn_filter) && Object.keys(jn_filter).length===0) {
        // A empty object at first level evaluate as true.
        return true;
    }
    return _kw_match_simple(kw, jn_filter, 0);
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
function kw_find_json_in_list(
    gobj,
    kw_list,
    item,
    flag
)
{
    if(!item || !is_array(kw_list)) {
        if(flag) {
            log_error(`item NULL or kw_list is not a list`);
        }
        return -1;
    }

    for(let i=0; i<kw_list.length; i++) {
        let jn_item = kw_list[i];
        if(json_is_identical(item, jn_item)) {
            return i;
        }
    }

    if(flag) {
        log_error(`item not found in this list`);
    }
    return -1;
}

/*************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
 *************************************************************/
function kw_select(kw, jn_filter, match_fn)
{
    if(!kw) {
        return null;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    let kw_new = [];

    if(is_array(kw)) {
        for(let i=0; i<kw.length; i++) {
            let jn_value = kw[i];
            if(match_fn(jn_value, jn_filter)) {
                kw_new.push(json_deep_copy(jn_value));
            }
        }
    } else if(is_object(kw)) {
        if(match_fn(kw, jn_filter)) {
            kw_new.push(json_deep_copy(kw));
        }
    } else {
        log_error("kw_select() BAD kw parameter");
        return null;
    }

    return kw_new;
}

/*************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where)
    If match_fn is 0 then kw_match_simple is used.
 *************************************************************/
function kw_collect(kw, jn_filter, match_fn)
{
    if(!kw) {
        return null;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    let kw_new = [];

    if(is_array(kw)) {
        for(let i=0; i<kw.length; i++) {
            let jn_value = kw[i];
            if(match_fn(jn_value, jn_filter)) {
                kw_new.push(jn_value);
            }
        }
    } else if(is_object(kw)) {
        if(match_fn(kw, jn_filter)) {
            kw_new.push(kw);
        }
    } else {
        log_error("kw_select() BAD kw parameter");
        return null;
    }

    return kw_new;
}

/*************************************************************
 *  From a dict,
 *  get a new dict with the same objects with only attributes in keys
 *  keys can be
 *          "$key"
 *         ["$key1", "$key2", ...]
 *         {"$key1":*, "$key2":*, ...}
 *************************************************************/
function kw_clone_by_keys(kw, keys)    // old filter_dict
{
    let new_dict = {};
    if(is_string(keys)) {
        let key = keys;
        if(kw.hasOwnProperty(key)) {
            new_dict[key] = kw[key];
        }
    } else if(is_array(keys)) {
        for(let j=0; j<keys.length; j++) {
            let key = keys[j];
            if(kw.hasOwnProperty(key)) {
                new_dict[key] = kw[key];
            }
        }
    } else if(is_object(keys)) {
        for(let key of Object.keys(keys)) {
            if(kw.hasOwnProperty(key)) {
                new_dict[key] = kw[key];
            }
        }
    }
    return new_dict;
}

/********************************************
 *  Get a local attribute
 ********************************************/
function kw_get_local_storage_value(key, default_value, create)
{
    if(!(key && window && window.JSON && window.localStorage)) {
        return undefined;
    }

    let value = window.localStorage.getItem(key);
    if(value === null || value===undefined) {
        if(create) {
            kw_set_local_storage_value(key, default_value);
        }
        return default_value;
    }

    try {
        value = JSON.parse(value);
    } catch (e) {
    }

    return value;
}

/********************************************
 *  Save a local attribute
 ********************************************/
function kw_set_local_storage_value(key, value)
{
    if(key && window && window.JSON && window.localStorage) {
        if(value !== undefined) {
            window.localStorage.setItem(key, JSON.stringify(value));
        }
    }
}

/********************************************
 *  Remove local attribute
 ********************************************/
function kw_remove_local_storage_value(key)
{
    if(key && window && window.localStorage) {
        window.localStorage.removeItem(key);
    }
}

/************************************************************
 *
 ************************************************************/
function id_index_in_obj_list(list, id) {
    if(!list) {
        return -1;
    }
    for(let i=0; i<list.length; i++) {
        if(list[i] && list[i].id === id) {
            return i;
        }
    }
    return -1;
}

/************************************************************
 *
 ************************************************************/
function elm_in_list(elm, list, case_insensitive) {
    if(!list) {
        log_error("ERROR: elm_in_list() list empty");
        return false;
    }
    if(!elm) {
        log_error("ERROR: elm_in_list() elm empty");
        return false;
    }
    for(let i=0; i<list.length; i++) {
        if(case_insensitive) {
            if(elm.toLowerCase() === list[i].toLowerCase()) {
                return true;
            }
        } else if(elm === list[i]) {
            return true;
        }
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function elms_in_list(elms, list, case_insensitive) {
    if(!list) {
        log_error("ERROR: elms_in_list() list empty");
        return false;
    }
    if(!elms) {
        log_error("ERROR: elms_in_list() elm empty");
        return false;
    }

    for(let i=0; i<elms.length; i++) {
        let elm = elms[i];
        if(elm_in_list(elm, list, case_insensitive)) {
            return true;
        }
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function delete_from_list(list, elm) {
    for(let i=0; i<list.length; i++) {
        if(elm === list[i]) {
            list.splice(i, 1);
            return true;
        }
    }
    return false; // elm does not exist!
}

/************************************************************
 *
 ************************************************************/
function msg_iev_read_key(kw, key)
{
    try {
        let __md_iev__ = kw["__md_iev__"];
        if(__md_iev__) {
            return __md_iev__[key];
        }
    } catch (e) {
        return undefined;
    }
    return undefined;
}

/************************************************************
 *
 ************************************************************/
function msg_iev_write_key(kw, key, value)
{
    let __md_iev__ = kw["__md_iev__"];
    if(!__md_iev__) {
        __md_iev__ = {};
        kw["__md_iev__"] = __md_iev__;
    }
    __md_iev__[key] = value;
}

/************************************************************
 *
 ************************************************************/
function msg_iev_push_stack(gobj, kw, stack, jn_data)
{
    if(!kw) {
        return;
    }

    let jn_stack = msg_iev_read_key(kw, stack);
    if(!jn_stack) {
        jn_stack = [];
        msg_iev_write_key(kw, stack, jn_stack);
    }
    try {
        // Code throwing an exception
        if(is_array(jn_stack)) {
            jn_stack.unshift(jn_data);
        }
    } catch(e) {
        log_error(e);
    }
}

/************************************************************
 *
 ************************************************************/
function msg_iev_get_stack(gobj, kw, stack, verbose)
{
    if(!is_object(kw)) {
        log_error(`kw is not a dict`);
        return null;
    }

    let jn_stack = msg_iev_read_key(kw, stack);
    if(!jn_stack) {
        if(verbose) {
            log_error(`stack NOT FOUND: ${stack}`);
        }
        return null;
    }

    if(jn_stack.length === 0) {
        if(verbose) {
            log_error(`stack empty: ${stack}`);
        }
        return null;
    }
    return jn_stack[0];
}

/************************************************************
 *          Load json file from server
 ************************************************************/
function _fileLoaded(xhr) {
    return xhr.status === 0 && xhr.responseText && xhr.responseURL.startsWith("file:");
}
function load_json_file(url, on_success, on_error)
{
    let req = new XMLHttpRequest();
    req.open("GET", url, true);
    req.setRequestHeader("Accept", "application/json");

    req.onreadystatechange = function () {
        if (req.readyState === 4) {
            if (req.status === 200 || _fileLoaded(req)) {
                try {
                    let json = JSON.parse(req.responseText);
                    on_success(json);
                } catch (error) {
                    if (on_error) {
                        log_error(`JSON parse status ${req.status},  error: ${error.message}`);
                        on_error(req.status);
                    }
                }
            } else {
                if (on_error) {
                    on_error(req.status);
                }
            }
        }
    };

    req.send();
}

/************************************************************
 *          Post http
 *          function on_response(status, response_data)
 ************************************************************/
function send_http_json_post(url, data, on_response) {

    let xhr = new XMLHttpRequest();
    xhr.open("POST", url);
    xhr.setRequestHeader("Accept", "application/json");
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            on_response(xhr.status, xhr.responseText);
        }
    };
    xhr.send(data);
}

/************************************************************
 *  callback (obj, key, value, full_path)
 ************************************************************/
function _traverse_dict(obj, callback, full_path)
{
    if(full_path === undefined || !is_string(full_path)) {
        full_path = "";
    }
    for (let key of Object.keys(obj)) {
        if(!obj.hasOwnProperty(key)) {
            continue;
        }
        let sufix = (full_path.length? "`":"") + key;
        full_path += sufix;

        callback.apply(this, [obj, key, obj[key], full_path]);

        if(is_object(obj[key])) {
            //going one step down in the object tree!!
            _traverse_dict(obj[key], callback, full_path);
        }

        full_path = full_path.slice(0, -sufix.length);
    }
}

/************************************************************
 *   Init json database
 *      Hierarchical tree. To use in webix style.
 *      If a record has childs,
 *      the own record has the key 'data' with the array of child records
 *
 *      Initialization
 *      --------------
        var jdb = {
            "type": [], // Can be [] or {}
            "hook": "data",
            "schema": { // topics
                "app_menu": [],
                "account_menu": []
            }
        };

        jdb_init(jdb);

 *
 *      Inside example
 *      --------------
 *      jdb: {
 *          hook: "data",
 *          type: []
 *          schema:{
 *              app_menu: [],
 *              account_menu: [],
 *              ...
 *          }
 *          topics: {
 *              app_menu: [
 *                  {
 *                      id:         // Mandatory field
 *                      icon:       // Remains: optional fields
 *                      value:
 *                      action:
 *                  },
 *                  {
 *                      id:
 *                      icon:
 *                      value:
 *                      action:
 *                      data: [
 *                          {
 *                              id:
 *                              icon:
 *                              value:
 *                              action:
 *                          },
 *
 *                      ]
 *                  },
 *                  ...
 *              ]
 *          }
 *      }
 ************************************************************/
function jdb_init(jdb, prefix, duplicate)
{
    if(duplicate) {
        jdb = json_deep_copy(jdb);
    }
    let type = kw_get_dict_value(jdb, "type", [], 1);
    let hook = kw_get_str(jdb, "hook", "data", 1);
    let schema = kw_get_dict_value(jdb, "schema", {}, 1);
    let topics = kw_get_dict_value(jdb, "topics", {}, 1);

    // Create topics defined in schema
    let walk = function(obj, key, value, full_path) {
        if(key.substring(0, 2) !== "__") {
            kw_get_dict_value(topics, full_path, json_deep_copy(type), 1);
            //trace_msg(sprintf("full_path: '%s', key: %s, value: %j", full_path, key, value));
        }
    };
    _traverse_dict(schema, walk, prefix);

    return jdb;
}

/********************************************
 *
 ********************************************/
function jdb_update(jdb, topic_name, path, kw)
{
    let topics = kw_get_dict_value(jdb, "topics", null, 0);
    let topic = kw_get_dict_value(topics, topic_name, null, 0);
    if(!topic) {
        log_error("jdb_update: topic not found: " + topic_name);
        return null;
    }
    if(empty_string(kw["id"])) {
        log_error("jdb_update: record without id: " + kw);
        trace_msg(kw);
        return null;
    }

    let ids = path.split("`");

    let id;
    let v = topic;
    while((id = ids.shift())) {
        if(empty_string(id) || !v) {
            break;
        }
        if(is_object(v)) {
            v = kw_get_dict_value(v, jdb.hook, [], 1);
        }
        v = _jdb_get(v, null, id, false);
    }

    if(ids.length===0 && v) {
        if(is_array(v)) {
            v.push(kw);
        } else {
            if(v["id"]===kw["id"]) {
                Object.assign(v, kw);
            } else {
                let v_ = kw_get_dict_value(v, jdb.hook, [], 1);
                let v__ = _jdb_get(v_, jdb.hook, kw["id"], false);
                if(v__) {
                    Object.assign(v__, kw);
                } else {
                    v_.push(kw);
                }
            }
        }
    }

    //trace_msg(JSON.stringify(topic, null, 4));

    return topic;
}

/********************************************
 *
 ********************************************/
function jdb_delete(jdb, topic_name, path, kw)
{
    let topics = kw_get_dict_value(jdb, "topics", null, 0);
    let topic = kw_get_dict_value(topics, topic_name, null, 0);
    if(!topic) {
        log_error("jdb_delete: topic not found: " + topic_name);
        return null;
    }
    if(empty_string(kw["id"])) {
        log_error("jdb_delete: record without id: " + kw);
        trace_msg(kw);
        return null;
    }

    let ids = path.split("`");

    let id;
    let v = topic;
    while((id = ids.shift())) {
        if(empty_string(id) || !v) {
            break;
        }
        if(is_object(v)) {
            v = kw_get_dict_value(v, jdb.hook, null, 0);
        }
        v = _jdb_get(v, null, id, false);
    }

    if(ids.length===0 && v) {
        if(is_array(v)) {
            let idx = id_index_in_obj_list(v, kw["id"]);
            if(idx >= 0) {
                v.splice(idx, 1);
            }
        } else {
            let v_ = kw_get_dict_value(v, jdb.hook, null, 0);
            let idx = id_index_in_obj_list(v_, kw["id"]);
            if(idx >= 0) {
                v_.splice(idx, 1);
            }
        }
    }

    //trace_msg(JSON.stringify(topic, null, 4));

    return topic;
}

/********************************************
 *
 ********************************************/
function jdb_get_topic(jdb, topic_name)
{
    let topics = kw_get_dict_value(jdb, "topics", null, 0, false);
    if(!topics) {
        log_error("jdb topics section not found");
        trace_msg(jdb);
        return null;
    }
    let topic = kw_get_dict_value(topics, topic_name, null, 0, false);
    if(!topic) {
        log_error("jdb topic not found: " + topic_name);
        trace_msg(topics);
        return null;
    }
    return topic;
}

/********************************************
 *
 ********************************************/
function jdb_get(jdb, topic_name, id, recursive)
{
    let topics = kw_get_dict_value(jdb, "topics", null, 0);
    let topic = kw_get_dict_value(topics, topic_name, null, 0);
    if(!topic) {
        log_error("jdb_get: topic not found: " + topic_name);
        return null;
    }
    if(recursive === undefined) {
        recursive = true;
    }
    return _jdb_get(topic, jdb.hook, id, recursive);
}

/********************************************
 *
 ********************************************/
function jdb_get_by_idx(jdb, topic_name, idx)
{
    let topics = kw_get_dict_value(jdb, "topics", null, 0);
    let topic = kw_get_dict_value(topics, topic_name, null, 0);
    if(!topic) {
        log_error("jdb_get_by_idx: topic not found: " + topic_name);
        return null;
    }
    if(idx < topic.length)  {
        return topic[idx];
    } else {
        return {};
    }
}

/**************************************************
 *  Busca el id en el arbol.
 *  Los id deben ser únicos, como requiere webix
 **************************************************/
function _jdb_get(v, hook, id, recursive)
{
    if(!v) {
        return null;
    }
    let j;
    let ln = v.length;
    for(j=0; j<ln; j++) {
        let v_ = v[j];
        if(v_) {
            let id_ = v_["id"];
            if(id_ && (id_ === id)) {
                return v_;
            }
            if(recursive) {
                if(kw_has_key(v_, hook)) {
                    let v__ = _jdb_get(v_[hook], hook, id, recursive);
                    if(v__) {
                        return v__;
                    }
                }
            }
        }
    }
    return null;
}

/***************************************************************************
 *
 *  type can be: str, int, real, bool, null, dict, list
 *  Example:

    static json_desc_t jn_xxx_desc[] = {
        // Name         Type        Default
        {"string",      "str",      ""},
        {"string2",     "str",      "Pepe"},
        {"integer",     "int",      "0660"},     // beginning with "0":octal,"x":hexa, others: integer
        {"boolean",     "bool",     "false"},
        {0}   // HACK important, final null
    };

 ***************************************************************************/
function create_json_record(json_desc, value) // here in js `json_desc` it's a dictionary with the defaults.
{
    let record = json_deep_copy(json_desc);  // Get fields and default values from json_desc
    json_object_update_existing(record, value); // Update (only with service fields) with user data

    return record;
}

/************************************************************
 * https://stackoverflow.com/questions/38552003/how-to-decode-jwt-token-in-javascript-without-using-a-library
 ************************************************************/
function jwtDecode(jwt)
{
    function b64DecodeUnicode(str) {
        return decodeURIComponent(atob(str).replace(/(.)/g, function (m, p) {
            let code = p.charCodeAt(0).toString(16).toUpperCase();
            if (code.length < 2) {
                code = '0' + code;
            }
            return '%' + code;
        }));
    }

    function decode(str) {
        let output = str.replace(/-/g, "+").replace(/_/g, "/");
        switch (output.length % 4) {
            case 0:
                break;
            case 2:
                output += "==";
                break;
            case 3:
                output += "=";
                break;
            default:
                throw "Illegal base64url string!";
        }

        try {
            return b64DecodeUnicode(output);
        } catch (err) {
            return atob(output);
        }
    }

    let jwtArray = jwt.split('.');

    return {
        header: decode(jwtArray[0]),
        payload: decode(jwtArray[1]),
        signature: decode(jwtArray[2])
    };
}

/************************************************************
 *  Decoded jwt and return in json
 *  'what' can be "header", "payload" or "signature"
 *  default is "payload"
 ************************************************************/
function jwt2json(jwt, what)
{
    try {
        let jwt_decoded = jwtDecode(jwt);
        switch(what) {
            case "header":
                return JSON.parse(jwt_decoded.header);
            case "signature":
                return JSON.parse(jwt_decoded.signature);
            case "payload":
            default:
                return JSON.parse(jwt_decoded.payload);
        }
    } catch (e) {
        return null;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
function createOneHtml(htmlString)
{
    // Convert the HTML string to DOM nodes
    const template = document.createElement('template');
    template.innerHTML = htmlString.trim(); // Trim to avoid extra text nodes

    return template.content.firstChild;
}

/***************************************************************************
 *  Code written by ChatGPT
    Example usage:

    function makeDraggable(handle, target) {
        let offsetX = 0;
        let offsetY = 0;

        // Called when the drag starts
        const onMouseDown = (e) => {
            // Calculate the initial offset
            offsetX = e.clientX - target.getBoundingClientRect().left;
            offsetY = e.clientY - target.getBoundingClientRect().top;

            // Attach the listeners to the document to allow for free movement
            document.addEventListener('mousemove', onMouseMove);
            document.addEventListener('mouseup', onMouseUp);
        };

        // Called when the mouse is moved
        const onMouseMove = (e) => {
            // Calculate the new position
            const x = e.clientX - offsetX;
            const y = e.clientY - offsetY;

            // Update the position of the target
            target.style.left = `${x}px`;
            target.style.top = `${y}px`;
        };

        // Called when the mouse button is released
        const onMouseUp = () => {
            // Remove the listeners from the document
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
        };

        // Attach the mousedown listener to the handle
        handle.addEventListener('mousedown', onMouseDown);
    }

    function addResizeFunctionality(resizeHandle, windowElement) {
        let startX, startY, startWidth, startHeight;

        const onMouseDown = (e) => {
            // Prevent default action (like text selection) during resize
            e.preventDefault();

            startX = e.clientX;
            startY = e.clientY;
            startWidth = parseInt(document.defaultView.getComputedStyle(windowElement).width, 10);
            startHeight = parseInt(document.defaultView.getComputedStyle(windowElement).height, 10);

            document.documentElement.addEventListener('mousemove', onMouseMove, false);
            document.documentElement.addEventListener('mouseup', onMouseUp, false);
        };

        const onMouseMove = (e) => {
            // Calculate new size
            const newWidth = startWidth + e.clientX - startX;
            const newHeight = startHeight + e.clientY - startY;

            // Update window size
            windowElement.style.width = `${newWidth}px`;
            windowElement.style.height = `${newHeight}px`;
        };

        const onMouseUp = () => {
            // Remove the listeners when resizing is done
            document.documentElement.removeEventListener('mousemove', onMouseMove, false);
            document.documentElement.removeEventListener('mouseup', onMouseUp, false);
        };

        // Attach the mousedown listener to the resize handle
        resizeHandle.addEventListener('mousedown', onMouseDown, false);
    }


    function createWindow({ title, x, y, width, height, parent }) {
        const windowEl = createElement2([
            'div',
            {
                class: 'window',
                // style: `position: absolute; left: ${x}px; top: ${y}px; width: ${width}px; height: ${height}px; display: flex; flex-direction: column; border: 1px solid black;`
                style: {
                    position: "absolute",
                    left: `${x}px`,
                    top: `${y}px`,
                    width: `${width}px`,
                    height: `${height}px`,
                    display: "flex",
                    "flex-direction": "column",
                    border: "1px solid black",
                }
            },
            [
                ['div', { class: 'window-top', style: 'display: flex; justify-content: space-between; padding: 5px; border-bottom: 1px solid black;cursor:move;' },
                    [
                        ['span', {}, title],
                        ['button', {}, 'Close', {
                            click: (e) => {
                                e.target.closest('.window').remove();
                            }
                        }]
                    ]
                ],
                ['div', { class: 'window-content', style: 'flex-grow: 1; padding: 5px;' }, 'This is the content area.'],
                ['div', { class: 'window-bottom', style: 'padding: 5px; border-top: 1px solid black; display: flex; justify-content: flex-end;' },
                    [
                        ['div', { class: 'resize-handle', style: 'width: 20px; height: 20px; cursor: nwse-resize;' }, '', {
                            mousedown: (e) => {
                                // Resize logic here
                            }
                        }]
                    ]
                ]
            ]
        ]);

        const resizeHandle = windowEl.querySelector('.resize-handle');

        makeDraggable(windowEl.querySelector('.window-top'), windowEl);
        addResizeFunctionality(resizeHandle, windowEl);

        parent.appendChild(windowEl);
    }


    // Example usage
    createWindow({
        title: 'My Draggable Window',
        x: 100,
        y: 100,
        width: 400,
        height: 300,
        parent: document.body
    });
 ***************************************************************************/


/***************************************************************************
 *  Create a HTMLElement:
 *      [tag, attrs, content, events]
 *
 *      tag     -> string
 *
 *      attrs   -> {key:string}
 *                  Special case for key=='style'
 *                      could be `style:'string'`
 *                      or `style:{k:s}`
 *                 Special case for key=='i18n' (or 'data-i18n' or 'data_i18next')
 *                      will be 'data-i18n' attr
 *
 *      content -> string | [createElement2() or parameters of createElement2]
 *                  if string begins with '<' then
 *                      it will be converted to HTMLElement
 *                          content = createOneHtml(string)
 *                  if attrs['i18n'] is not empty then (or 'data-i18n' or 'data_i18next')
*                          it will be overridden with the translation
 *                          content = translate(attrs['i18n']));
 *
 *      events  -> {event: ()}
 *
 *  Return the created element
 *
 *  Examples:
 *      ['div', { class: 'window-top', style: 'border-bottom: 1px solid black;' },
 *          [
 *              ['span', {}, 'title'],
 *              ['button', {}, 'Close', {
 *                  click: (e) => {
 *                      e.target.closest('.window').remove();
 *                  }
 *              }]
 *          ]
 *      ]
 *
 *      ['span', {style: {position: 'absolute'} }, 'title'],
 *
 ***************************************************************************/
function createElement2(description, translate_fn) {
    let [tag, attrs, content, events] = description;

    /*
     *  Create the html element
     */
    if (!tag) {
        log_error("Tag must be a valid HTML element.");
        return;
    }
    let el = document.createElement(tag);

    /*
     *  Append attributes
     */
    let data_i18next = '';

    if(attrs && typeof attrs === 'object') {
        for (let attr of Object.keys(attrs)) {
            if (attr === 'style' && typeof attrs[attr] === 'object') {
                // Convert the style object to a CSS string
                let styleString = '';

                for(const prop of Object.keys(attrs[attr])) {
                    styleString += `${prop}: ${attrs[attr][prop]};`;
                }
                el.style.cssText = styleString;
            } else if (attr === 'style') {
                el.style.cssText = attrs[attr];
            } else {
                if(attr === 'i18n' || attr === 'data-i18n' ||
                        attr === 'data_i18next' || attr === 'data-i18next') {
                    data_i18next = attrs[attr];
                    el.setAttribute('data-i18n', data_i18next);
                } else {
                    el.setAttribute(attr, attrs[attr]);
                }
            }
        }
    }

    /*
     *  Append content, it can be a string or another kids
     */
    if(is_string(content)) {
        content = content.trim();
        if(content.length > 0 && content[0] === '<') {
            el.appendChild(createOneHtml(content));
        } else {
            if(translate_fn && data_i18next) {
                el.textContent = translate_fn(data_i18next);
            } else {
                el.textContent = content;
            }
        }
    } else if(is_array(content)) {
        if(content.length > 0) {
            if(is_string(content[0])) {
                el.appendChild(createElement2(content, translate_fn));
            } else {
                for (let child of content) {
                    // Check if the child is an array description or an HTMLElement
                    if (Array.isArray(child) && child.length > 0) {
                        el.appendChild(createElement2(child, translate_fn));
                    } else if (child instanceof HTMLElement) {
                        el.appendChild(child);
                    }
                }
            }
        }
    } else if(content instanceof HTMLElement ||
            content instanceof Text ||
            content instanceof SVGElement)
    {
        el.appendChild(content);
    } else {
        if(content !== undefined) {
            log_error("content ignored");
            trace_json(content);
        }
    }

    /*
     *  Attach event listeners
     */
    if (events && typeof events === 'object') {
        for(const eventType of Object.keys(events)) {
            el.addEventListener(eventType, events[eventType]);
        }
    }

    return el;
}

/***************************************************************************
 *  Code written by ChatGPT
    // Usage example:
    var element = document.getElementById("myElement");
    var position = getPositionRelativeToBody(element);
    console.log("Top: " + position.top + ", Left: " + position.left + ", Right: " + position.right + ", Bottom: " + position.bottom);
 ***************************************************************************/
function getPositionRelativeToBody(element)
{
    let rect = element.getBoundingClientRect();
    let scrollTop = window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop;
    let scrollLeft = window.pageXOffset || document.documentElement.scrollLeft || document.body.scrollLeft;

    let top = rect.top + scrollTop;
    let left = rect.left + scrollLeft;
    let right = left + rect.width;
    let bottom = top + rect.height;

    return {
        top: top,
        left: left,
        right: right,
        bottom: bottom
    };
}

/***************************************************************************
 * Create SVG element from string code
 ***************************************************************************/
function parseSVG(string)
{
    return ((new DOMParser().parseFromString(string, 'image/svg+xml')).firstChild);
}

/************************************************************
 * Example Usage
 *     const tracker = timeTracker("Track1");
 *     tracker.mark("Step 1");
 *     tracker.mark("Step 2");
 ************************************************************/
function timeTracker(tracker_name="Time Tracker", verbose=false)
{
    const name = tracker_name;
    const startTime = performance.now();
    let lastMarkTime = startTime;

    if(verbose) {
        log_warning(
            `--> ${name} - starting`
        );
    }

    return {
        mark: (label = "Intermediate", result="") => {
            const currentTime = performance.now();
            const partial = (currentTime - lastMarkTime) / 1000;
            const total = (currentTime - startTime) / 1000;
            if(verbose) {
                log_warning(
                    `--> ${name} ${label} - partial: ${partial} sec, total: ${total} sec, ${result}`
                );
            }
            lastMarkTime = currentTime;
            return {
                partial: partial,
                total: total
            };
        }
    };
}

/************************************************************
 *  Get the current timestamp in ISO 8601 format with milliseconds and time zone.
 *  Example Output: "2024-02-24T14:05:30.123+0200"
 ************************************************************/
function current_timestamp()
{
    const now = new Date();

    // Format YYYY-MM-DDTHH:mm:ss
    const timestamp = now.toISOString().slice(0, 19);

    // Get milliseconds (3-digit nanosecond equivalent)
    const milliseconds = String(now.getMilliseconds()).padStart(3, "0");

    // Get time zone offset in ±HHMM format
    const offsetMinutes = now.getTimezoneOffset();
    const sign = offsetMinutes > 0 ? "-" : "+";
    const hours = String(Math.abs(offsetMinutes) / 60).padStart(2, "0");
    const minutes = String(Math.abs(offsetMinutes) % 60).padStart(2, "0");
    const timezone = `${sign}${hours}${minutes}`;

    return `${timestamp}.${milliseconds}${timezone}`;
}

/********************************************
 *  Return UTC time of right now
 ********************************************/
function get_now()
{
    return Math.floor(Date.now() / 1000);
}

/************************************************************
 *
 ************************************************************/
function index_in_list(list, elm) {
    if(!list) {
        throw "ERROR: index_in_list() list empty";
    }
    for(let i=0; i<list.length; i++) {
        if(elm === list[i]) {
            return i;
        }
    }
    return -1;
}

/************************************************************
 *
 ************************************************************/
function node_uuid()
{
    let uuid = "";

    if(window) {
        uuid = window.localStorage.getItem("host_uuid");
        if (!uuid) {
            uuid = window.crypto.randomUUID();  // Generate a new UUID
            window.localStorage.setItem("host_uuid", uuid);
        }
    }
    return uuid;
}


export {
    kw_flag_t,
    json_deep_copy,
    duplicate_objects,
    json_is_identical,
    json_object_update,
    json_object_update_existing,
    json_object_update_missing,
    json_object_get,
    json_object_set_new,
    json_object_del,
    json_array_append,
    json_array_append_new,
    json_array_remove,
    json_array_extend,
    json_object_size,
    json_array_size,
    json_size,

    is_object,
    is_array,
    is_string,
    is_number,
    is_boolean,
    is_null,
    is_pure_number,
    is_date,
    is_function,
    is_gobj,

    empty_string,
    strncmp,
    strcmp,
    strcasecmp,
    cmp_two_simple_json,
    strstr,

    is_metadata_key,
    is_private_key,
    kw_pop,
    kw_has_key,
    kw_find_path,
    kw_get_bool,
    kw_get_int,
    kw_get_real,
    kw_get_str,
    kw_get_pointer,
    kw_get_dict,
    kw_get_list,
    kw_get_dict_value,
    kw_set_dict_value,
    kw_set_subdict_value,
    kw_match_simple,
    kw_find_json_in_list,
    kw_select,
    kw_collect,
    kw_clone_by_keys,

    kw_get_local_storage_value,
    kw_set_local_storage_value,
    kw_remove_local_storage_value,

    id_index_in_obj_list,
    elm_in_list,
    elms_in_list,
    delete_from_list,

    msg_iev_read_key,
    msg_iev_write_key,
    msg_iev_push_stack,
    msg_iev_get_stack,

    load_json_file,
    send_http_json_post,

    jdb_init,
    jdb_update,
    jdb_delete,
    jdb_get_topic,
    jdb_get,
    jdb_get_by_idx,

    create_json_record,

    set_log_functions,
    log_error,
    log_warning,
    log_info,
    log_debug,
    trace_msg,
    trace_json,

    jwtDecode,
    jwt2json,

    createOneHtml,
    createElement2,
    getPositionRelativeToBody,
    parseSVG,

    timeTracker,
    current_timestamp,
    get_now,
    index_in_list,
    node_uuid,
};
