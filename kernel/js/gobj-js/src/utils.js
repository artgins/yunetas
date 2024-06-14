/*********************************************************************************
 *        Auxiliary functions
 *  Ni puta idea de javascript.
 *  La parte para simular clases es copiado. Casi acierto a la primera oye.
 *  Lo siento, solo quería crear una clase, pero ha salido esta cosa.
 *  Con poder programar con mis gobjs tengo suficiente.
 *
 *  Last revision:
 *      20 Jun 2014 - Upgraded to yuneta api. Nothing to do.
 *      15 Jul 2015 - Upgraded to yuneta 1.0.0.
 *      Jun 2024    - many new functions
 *
 *********************************************************************************/

/**************************************************************************
 *        Utils
 **************************************************************************/
(function (exports) {
    "use strict";

    /************************************************************
     *
     ************************************************************/
    Function.prototype.__makeSubclass__ = function() {

        function Class() {
            if (!(this instanceof Class)) {
                  throw("Constructor called without new");
            }
            if ("__init__" in this) {
                this.__init__.apply(this, arguments);
            }
        }
        Function.prototype.__makeSubclass__.nonconstructor.prototype= this.prototype;
        Class.prototype= new Function.prototype.__makeSubclass__.nonconstructor();
        return Class;
    };
    Function.prototype.__makeSubclass__.nonconstructor= function() {};

    /************************************************************
     *  Clona arrays y objects, remains return the same.
        WARNING

        **duplicate** is a copy with new references

        **clone** is a copy with incref references
     ************************************************************/
    function __duplicate__(obj)
    {
        let clonedObjectsArray = [];
        let originalObjectsArray = []; //used to remove the unique ids when finished
        let next_objid = 0;

        function objectId(obj) {
            if (obj == null) return null;
            if (obj.__obj_id == undefined) {
                obj.__obj_id = next_objid++;
                originalObjectsArray[obj.__obj_id] = obj;
            }
            return obj.__obj_id;
        }

        function cloneRecursive(obj) {
            if (null == obj || typeof obj == "string" || typeof obj == "number" || typeof obj == "boolean") return obj;

            // Handle Date
            if (obj instanceof Date) {
                let copy = new Date();
                copy.setTime(obj.getTime());
                return copy;
            }

            // Handle Array
            if (obj instanceof Array) {
                let copy = [];
                for (let i = 0; i < obj.length; ++i) {
                    copy[i] = cloneRecursive(obj[i]);
                }
                return copy;
            }

            // Handle Object
            if (obj instanceof Object) {
                if (clonedObjectsArray[objectId(obj)] != undefined)
                    return clonedObjectsArray[objectId(obj)];

                let copy;
                if (obj instanceof Function)//Handle Function
                    copy = function(){return obj.apply(this, arguments);};
                else
                    copy = {};

                clonedObjectsArray[objectId(obj)] = copy;

                for (let attr in obj)
                    if (attr != "__obj_id" && obj.hasOwnProperty(attr))
                        copy[attr] = cloneRecursive(obj[attr]);

                return copy;
            }

            throw new Error("Unable to copy obj! Its type isn't supported.");
        }
        let cloneObj = cloneRecursive(obj);

        //remove the unique ids
        for (let i = 0; i < originalObjectsArray.length; i++) {
            delete originalObjectsArray[i].__obj_id;
        }

        return cloneObj;
    }

    /************************************************************
     *  Update a dict with another dict: ONLY existing items!! (YES recursive)
     ************************************************************/
    function json_object_update_existing_recursive(destination, source) {
        if(!source) {
            return destination;
        }
        for (let property in source) {
            if (source.hasOwnProperty(property) && destination.hasOwnProperty(property)) {
                if(is_object(destination[property]) && is_object(source[property])) {
                    json_object_update_existing_recursive(
                        destination[property],
                        source[property]
                    );
                } else {
                    destination[property] = source[property];
                }
            }
        }
        return destination;
    }

    /************************************************************
     *  Update a dict with another dict: ONLY existing items!! (NOT recursive)
     *  Like json_object_update_existing()
     ************************************************************/
    function __update_dict__(destination, source) {
        if(!source) {
            return destination;
        }
        for (var property in source) {
            if (source.hasOwnProperty(property) && destination.hasOwnProperty(property)) {
                destination[property] = source[property];
            }
        }
        return destination;
    }

    /************************************************************
     *  Extend a dict with another dict (NOT recursive),
     *  adding new keys and overwriting existing keys.
     *  Like json_object_update()
     ************************************************************/
    function __extend_dict__(destination, source) {
        if(!source) {
            return destination;
        }
        for (var property in source) {
            if (source.hasOwnProperty(property)) {
                destination[property] = source[property];
            }
        }
        return destination;
    }

    /************************************************************
     *  Extend array
     ************************************************************/
    function __extend_list__(destination, source) {
        if(!source) {
            return destination;
        }
        Array.prototype.push.apply(destination, source);
        return destination;
    }


    /************************************************************
     *  Update a dict with another dict: ONLY missing items!! (NOT recursive)
     *  Like json_object_update_missing()
     ************************************************************/
    function json_object_update_missing(destination, source) {
        if(!source) {
            return destination;
        }
        for (var property in source) {
            if(source.hasOwnProperty(property) && !destination.hasOwnProperty(property)) {
                destination[property] = source[property];
            }
        }
        return destination;
    }

    /************************************************************
     *
     ************************************************************/
    function array_real_length(list)
    {
        var ln = 0;
        for(var i in list) {
            if(list.hasOwnProperty(i)) { // TODO review, dudes
                ln++;
            }
        }
        return ln;
    }

    /************************************************************
     * Finds the index of the element in the array.
     *
     * @param {Function} elm Element to look for.
     * @param {Function[]} list Array to search through.
     * @return {Number} Index of the specified elm, -1 if not found
     ************************************************************/
    function index_of_list(elm, list) {
        // Existence of a native index
        let nativeIndexOf = list.indexOf? true : false;

        // Return the index via the native method if possible
        if(nativeIndexOf) {
            return list.indexOf(elm);
        }

        // There is no native method
        // Use a manual loop to find the index
        let i = list.length;
        while(i--) {
            // If the elm matches, return it is index
            if(list[i] === elm) {
                return i;
            }
        }

        // Default to returning -1
        return -1;
    }

    /************************************************************
     *
     ************************************************************/
    function elm_in_list(elm, list) {
        if(!list) {
            throw "ERROR: elm_in_list() list empty";
        }
        if(!elm) {
            throw "ERROR: elm_in_list() elm empty";
        }
        for(var i=0; i<list.length; i++) {
            if(elm === list[i]) {
                return true;
            }
        }
        return false;
    }

    /************************************************************
     *
     ************************************************************/
    function elms_in_list(elms, list) {
        if(!list) {
            throw "ERROR: elms_in_list() list empty";
        }
        if(!elms) {
            throw "ERROR: elms_in_list() elm empty";
        }

        for(var i=0; i<elms.length; i++) {
            var elm = elms[i];
            if(elm_in_list(elm, list)) {
                return true;
            }
        }
        return false;
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
    function get_object_from_list(list, id) {

        if(!list) {
            return null;
        }
        for(let i=0; i<list.length; i++) {
            if(list[i] && list[i].id == id) {
                return list[i];
            }
        }
        return null;
    }

    /************************************************************
     *
     ************************************************************/
    function none_in_list(list) {
        for(let i=0; i<list.length; i++) {
            if(!list[i]) {
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
    function same_list(arrA, arrB) {
        //check if lengths are different
        if(arrA.length !== arrB.length) {
            return false;
        }

        //slice so we do not effect the orginal
        //sort makes sure they are in order
        let cA = arrA.slice().sort();
        let cB = arrB.slice().sort();

        for(let i=0;i<cA.length; i++) {
            if(cA[i]!==cB[i]) {
                return false;
            }
        }
        return true;
    }

    /************************************************************
     *
     ************************************************************/
    function __strip__(s){
        return ( s || "" ).replace( /^\s+|\s+$/g, "" );
    }

    /************************************************************
     *
     ************************************************************/
    function __set__(arr) {
        let seen = {},
            result = [];
        let len = arr.length;
        for (let i = 0; i < len; i++) {
            let el = arr[i];
            if (!seen[el]) {
                seen[el] = true;
                result.push(el);
            }
        }
        return result;
    }

    /************************************************************
     *
     ************************************************************/
    function get_function_name(func) {
        var fName = null;
        if (typeof func === "function" || typeof func === "object") {
            fName = ("" + func).match(/function\s*([\w\$]*)\s*\(/);
        }
        if (fName !== null) {
            return fName[1] + "()";
        }
        return "";
    }

    /************************************************************
     *      Array Remove - By John Resig (MIT Licensed)
     *      Same as utils.delete_from_list() ?
     ************************************************************/
    Array.prototype.remove = function(from, to) {
      var rest = this.slice((to || from) + 1 || this.length);
      this.length = from < 0 ? this.length + from : from;
      return this.push.apply(this, rest);
    };

    /************************************************************
     *      String repeat
     *      http://stackoverflow.com/questions/202605/repeat-string-javascript
     ************************************************************/
    String.prototype.repeat = function(count) {
        if (count < 1) return "";
        var result = "", pattern = this.valueOf();
        while (count > 1) {
            if (count & 1) {
                result += pattern;
            }
            count >>= 1;
            pattern += pattern;
        }
        result += pattern;
        return result;
    };

    /************************************************************
     *      Like C functions
     ************************************************************/
    function strncmp (str1, str2, lgth)
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

        return ( ( s1 == s2 ) ? 0 : ( ( s1 > s2 ) ? 1 : -1 ) );
    }


    /************************************************************
     *
     ************************************************************/
    // Return if a value is an object
    function is_object(a)
    {
        return (!!a) && (a.constructor === Object);
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

        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    // Return if a value is an array
    function is_array(a)
    {
        return (!!a) && (a.constructor === Array);
    }

    /************************************************************
     *
     ************************************************************/
    // Return if a value is a string
    function is_string(value)
    {
        return typeof value === "string" || value instanceof String;
    }

    /************************************************************
     *
     ************************************************************/
    // Return if a value is a number
    function is_number(value)
    {
        return typeof value === "number" && isFinite(value);
    }

    /************************************************************
     *
     ************************************************************/
    // Return if a value is a boolean
    function is_boolean(value)
    {
        return value === false || value === true;
    }

    /************************************************************
     *
     ************************************************************/
    // Return if a value is a null
    function is_null(value)
    {
        return value === null || value === undefined;
    }

    /************************************************************
     *
     ************************************************************/
    function is_date(value)
    {
        return value instanceof Date;
    }

    /************************************************************
     *
     ************************************************************/
    function is_function(value)
    {
        return typeof value === "function";
    }

    /************************************************************
     *
     ************************************************************/
    function is_gobj(value)
    {
        return value instanceof GObj;
    }

    /************************************************************
     *
     ************************************************************/
    function empty_string(s)
    {
        if(!s || typeof(s) !== "string") {
            return true;
        }

        if(s.length == 0) {
            return true;
        }
        return false;
    }

    /************************************************************
     *
     ************************************************************/
    function match_dict_list_by_kw(set, __filter__)
    {
        if(!__filter__) {
            return [];
        }
        return set.filter(function (entry) {
            return Object.keys(__filter__).every(function (key) {
                if(!entry.hasOwnProperty(key)) {
                    return false;
                }
                return entry[key] === __filter__[key];
            });
        });
    }

    /************************************************************
     *
     ************************************************************/
    function kw_is_identical(kw1, kw2)
    {
        var kw1_ = JSON.stringify(kw1);
        var kw2_ = JSON.stringify(kw2);
        return (kw1_ == kw2_)? true: false;
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

        return ( ( str1 == str2 ) ? 0 : ( ( str1 > str2 ) ? 1 : -1 ) );
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
        let ret = strcmp(val1, val2);
        return ret;
    }

    /************************************************************
     *
     ************************************************************/
    function _kw_match_simple(kw, jn_filter, level)
    {
//         for (var key in __filter__) {
//             if (!kw.hasOwnProperty(key)) {
//                 return false; // si el filtro no exite en kw, fuera
//             }
//             if(kw[key] != __filter__[key]) {
//                 return false;
//             }
//         }
//         return true;

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
            if(json_object_size(jn_filter)==0) {
                // Empty object evaluate as false.
                matched = false;
            } else {
                // Not Empty object evaluate as true, until a NOT match condition occurs.
                matched = true;
            }

            for(let filter_path in jn_filter) {
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
                if(op == "__equal__") { // TODO __equal__ by default
                    let cmp = cmp_two_simple_json(jn_record_value, jn_filter_value);
                    if(cmp!=0) {
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

    /************************************************************
     *
     ************************************************************/
    function kw_match_simple(kw, jn_filter)
    {
        if(!jn_filter) {
         // Si no hay filtro pasan todos.
           return true;
        }
        if(is_object(jn_filter) && Object.keys(jn_filter).length==0) {
            // A empty object at first level evaluate as true.
            return true;
        }
        return _kw_match_simple(kw, jn_filter, 0);
    }

    /*************************************************************
        Being `kw` a row's list or list of dicts [{},...],
        return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
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
        var kw_new = [];

        if(is_array(kw)) {
            for(var i=0; i<kw.length; i++) {
                var jn_value = kw[i];
                if(match_fn(jn_value, jn_filter)) {
                    kw_new.push(jn_value);
                }
            }
        } else if(is_object(kw)) {
            if(match_fn(kw, jn_filter)) {
                kw_new.push(kw);
            }
        } else {
            log_error("kw_collect() BAD kw parameter");
            return null;
        }

        return kw_new;
    }

    /*************************************************************
        Utility for databases.
        Being `ids` a:

            "$id"

            {
                "$id": {
                    "id": "$id",
                    ...
                }
                ...
            }

            ["$id", ...]

            [
                "$id",
                {
                    "id":$id,
                    ...
                },
                ...
            ]

        return a list of all ids
    *************************************************************/
    function kwid_get_ids(ids)
    {
        if(!ids) {
            return [];
        }

        let new_ids = [];

        if(is_string(ids)) {
            /*
                "$id"
            */
            new_ids.push(ids);
        } else if(is_object(ids)) {
            /*
                {
                    "$id": {
                        "id": "$id",
                        ...
                    }
                    ...
                }
            */
            for(let id in ids) {
                if (ids.hasOwnProperty(id)) {
                    new_ids.push(id);
                }
            }
        } else if(is_array(ids)) {
            ids.forEach(function(item) {
                if(is_string(item)) {
                    /*
                        ["$id", ...]
                     */
                    if(!empty_string(item)) {
                        new_ids.push(item);
                    }
                } else if(is_object(item)) {
                    /*
                        [
                            {
                                "id":$id,
                                ...
                            },
                            ...
                        ]
                     */
                    let id = kw_get_str(item, "id", 0, 0);
                    if(id) {
                        new_ids.push(id);
                    }
                }
            });
        }

        return new_ids;
    }

    /*************************************************************
        Utility for databases.
        Return TRUE if `id` is in the list/dict/str `ids`
     *************************************************************/
    function kwid_match_id(ids, id)
    {
        if(is_null(ids) || is_null(id)) {
            // Si no hay filtro pasan todos.
            return true;
        }

        if(is_array(ids)) {
            if(ids.length==0) {
                // A empty object at first level evaluate as true.
                return true;
            }
            for(let i=0; i<ids.length; i++) {
                let value = ids[i];
                if(value == id) {
                    return true;
                }
            }

        } else if(is_object(ids)) {
            if(Object.keys(ids).length==0) {
                // A empty object at first level evaluate as true.
                return true;
            }
            for(let key in ids) {
                if(key == id) {
                    return true;
                }
            }

        } else if(is_string(ids)) {
            if(ids == id) {
                return true;
            }
        }

        return false;
    }

    /*************************************************************
        Utility for databases.
        Being `kw` a:
            - list of strings [s,...]
            - list of dicts [{},...]
            - dict of dicts {id:{},...}
        return a **NEW** list of incref (clone) kw filtering the rows by `jn_filter` (where),
        and matching the ids.
        If match_fn is 0 then kw_match_simple is used.
        NOTE Using JSON_INCREF/JSON_DECREF HACK
     *************************************************************/
    function kwid_collect(kw, ids, jn_filter, match_fn)
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

                let id;
                if(is_object(jn_value)) {
                    id = kw_get_str(jn_value, "id", "", false);
                } else if(is_string(jn_value)) {
                    id = jn_value;
                } else {
                    continue;
                }

                if(!kwid_match_id(ids, id)) {
                    continue;
                }
                if(match_fn(jn_value, jn_filter)) {
                    kw_new.push(jn_value);
                }
            }
        } else if(is_object(kw)) {
            for(let id in kw) {
                let jn_value = kw[id];

                if(!kwid_match_id(ids, id)) {
                    continue;
                }
                if(match_fn(jn_value, jn_filter)) {
                    kw_new.push(jn_value);
                }
            }

        } else  {
            log_error("kw_collect() BAD kw parameter");
            return null;
        }

        return kw_new;
    }

    /*************************************************************
        Utility for databases.
        Return a new dict from a "dict of records" or "list of records"
        WARNING the "id" of a dict's record is hardcorded to their key.
        Convention:
            - all arrays are list of records (dicts) with "id" field as primary key
            - delimiter is '`' and '.'
        If path is empty then use kw
     *************************************************************/
    function kwid_new_dict(kw, path)
    {
        let new_dict = {};
        if(!empty_string(path)) {
            kw = _kw_search_path(kw, path);
        }
        if(is_object(kw)) {
            new_dict = kw;

        } else if(is_array(kw)) {
            for(let i=0; i<kw.length; i++) {
                let kv = kw[i];
                let id = kw_get_str(kv, "id", null, false);
                if(!empty_string(id)) {
                    new_dict[id] = kv;
                }
            }

        } else {
            log_error("kwid_new_dict: data type unknown");
        }

        return new_dict;
    }

    /************************************************************
     *  kw can be a dict or a list
     *  dict: return the first key
     *  list: return the first item
     ************************************************************/
    function kwid_get_first_id(kw)
    {
        let list = kwid_get_ids(kw);
        if(list.length > 0) {
            return list[0];
        }

        return null;
    }

    /*************************************************************
     *  Utility for databases. See kwid_collect parameters
     *************************************************************/
    function kwid_find_one_record(kw, ids, jn_filter, match_fn)
    {
        let list = kwid_collect(kw, ids, jn_filter, match_fn);
        if(list.length > 0) {
            return list[0];
        } else {
            return null;
        }
    }

    /*************************************************************
     *  Utility for databases
     *************************************************************/
    function kwid_delete_record(kw, ids_)
    {
        let deletes = 0;
        let ids = kwid_get_ids(ids_);

        for(let i=0; i<ids.length; i++) {
            let id = ids[i];
            if(is_object(kw)) {
                delete kw[id];
                deletes++;
            } else if(is_array(kw)) {
                for(let j=0; j<kw.length; j++) {
                    if(id == kw[j].id) {
                        delete_from_list(kw, kw[j]);
                        deletes++;
                        j = -1;
                        continue;
                    }
                }
            } else {
                log_error("kwid_delete_record: data type unknown");
            }
        }
        return deletes>0?0:-1;
    }

    /*************************************************************
     *  From a dict,
     *  get a new dict with the same objects with only attributes in keylist
     *  keylist can be a [s,...] of {s:..., ...}
     *************************************************************/
    function filter_dict(dict, keylist)
    {
        let new_dict = {};
        if(is_array(keylist)) {
            for(let j=0; j<keylist.length; j++) {
                let key = keylist[j];
                if(dict.hasOwnProperty(key)) {
                    new_dict[key] = dict[key];
                }
            }
        } else if(is_object(keylist)) {
            for(let key in keylist) {
                if(dict.hasOwnProperty(key)) {
                    new_dict[key] = dict[key];
                }
            }
        }
        return new_dict;
    }

    /*************************************************************
     *  From a list of objects (dict_list),
     *  get a new list with the same objects with only attributes in keylist
     *************************************************************/
    function filter_dictlist(dict_list, keylist)
    {
        let new_dictlist = [];
        for(let i=0; i<dict_list.length; i++) {
            let new_dict = filter_dict(dict_list[i], keylist);
            new_dictlist.push(new_dict);
        }
        return new_dictlist;
    }

    /*************************************************************
     *  From a list of objects (dict_list),
     *  get a new list with the value of the `key` attribute
     *************************************************************/
    function filter_list(dict_list, key)
    {
        let new_list = [];
        for(let i=0; i<dict_list.length; i++) {
            let dict = dict_list[i];
            if(dict.hasOwnProperty(key)) {
                new_list.push(dict[key]);
            }
        }
        return new_list;
    }

    /************************************************************
     *   [{id:..}, ...] convierte en "id,id2,..."
     ************************************************************/
    function ids2str(dictlist)
    {
        let s = "";

        for(let i=0; i<dictlist.length; i++) {
            let dict = dictlist[i];
            if(i==0) {
                s += dict.id;
            } else {
                s += "," + dict.id;
            }
        }
        return s;
    }

    /************************************************************
     *  OLD msgx_read_MIA_key
     ************************************************************/
    function msg_iev_read_key(kw, key, create, default_value) // TODO create, default_value
    {
        try {
            var __md_iev__ = kw["__md_iev__"];
            if(__md_iev__) {
                return __md_iev__[key];
            }
        } catch (e) {
            return undefined;
        }
        return undefined;
    }

    /************************************************************
     *  OLD msgx_write_MIA_key
     ************************************************************/
    function msg_iev_write_key(kw, key, value)
    {
        var __md_iev__ = kw["__md_iev__"];
        if(!__md_iev__) {
            __md_iev__ = {};
            kw["__md_iev__"] = __md_iev__;
        }
        __md_iev__[key] = value;
    }

    /************************************************************
     *  OLD msgx_delete_MIA_key
     ************************************************************/
    function msg_iev_delete_key(kw, key)
    {
        var __md_iev__ = kw["__md_iev__"];
        if(!__md_iev__) {
            // log error?
            return;
        }
        if(!kw_has_key(__md_iev__, key)) {
            // log error?
            return;
        }
        delete __md_iev__[key];
    }

    /************************************************************
     *
     ************************************************************/
    function msg_delete_MIA(kw, key)
    {
        var __md_iev__ = kw["__md_iev__"];
        if(!__md_iev__) {
            // log error?
            return;
        }
        delete kw["__md_iev__"];
    }

    /************************************************************
     *  Apply answer filters
     ************************************************************/
    var message_area_filters = [];
    function msg_iev_add_answer_filter(gobj, field_name, answer_filter_cb)
    {
        var len = message_area_filters.length;
        for(var i=0; i<len; i++) {
            var name = message_area_filters[i].field_name;
            if(name == field_name) {
                return 0; // already registered
            }
        }
        var filter = {
            field_name: field_name,
            answer_filter_fn: answer_filter_cb,
            gobj: gobj
        };
        message_area_filters.push(filter);
    }

    /************************************************************
     *  Apply answer filters
     ************************************************************/
    function msg_apply_answer_filters(kw_answer, __md_iev__, src)
    {
        var len = message_area_filters.length;
        for(var i=0; i<len; i++) {
            var name = message_area_filters[i].field_name;
            var cb = message_area_filters[i].answer_filter_fn;
            var gobj = message_area_filters[i].gobj;
            if(kw_has_key(__md_iev__, name)) {
                /*
                 *  Filter
                 */
                var value = kw_get_dict_value(__md_iev__, name, 0);
                (cb)(gobj, kw_answer, name, value, src);
            }
        }
    }

    /************************************************************
     *  Build the answer message
     *  with the id area of the request message
     ************************************************************/
    function msg_iev_answer(gobj, kw, kw_answer)
    {
        try {
            let __md_iev__ = kw["__md_iev__"];
            if(__md_iev__) {
                let new_msg_area = Object(__md_iev__); // WARNING new_msg_area is not a clone, is the same obj
                kw_answer["__md_iev__"] = new_msg_area;
                msg_apply_answer_filters(kw_answer, new_msg_area, gobj);
            }
        } catch (e) {
        }

        return kw_answer;
    }

    /************************************************************
     *
     ************************************************************/
    function msg_iev_push_stack(kw, stack, user_info)
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
                jn_stack.unshift(user_info);
            }
        } catch(e) {
            console.log(e);
        }
    }

    /************************************************************
     *
     ************************************************************/
    function msg_iev_get_stack(kw, stack)
    {
        if(!kw) {
            return null;
        }

        var jn_stack = msg_iev_read_key(kw, stack);
        if(!jn_stack) {
            return 0;
        }

        if(jn_stack.length == 0) {
            return null;
        }
        return jn_stack[0];
    }

    /************************************************************
     *
     ************************************************************/
    function msg_iev_pop_stack(kw, stack)
    {
        if(!kw) {
            return null;
        }

        let jn_stack = msg_iev_read_key(kw, stack);
        if(!jn_stack) {
            return 0;
        }

        if(jn_stack.length == 0) {
            return null;
        }
        let user_info = jn_stack.shift();
        if(jn_stack.length == 0) {
            msg_iev_delete_key(kw, stack);
        }
        return user_info;
    }

    /************************************************************
     *
     ************************************************************/
    var msg_type_list = [
        "__command__",
        "__publishing__",
        "__subscribing__",
        "__unsubscribing__",
        "__query__",
        "__response__",
        "__order__",
        "__first_shot__"
    ];

    function msg_set_msg_type(kw, msg_type)
    {
        if(!empty_string(msg_type)) {
            if(is_metadata_key(msg_type) && !elm_in_list(msg_type, msg_type_list)) {
                // HACK If it's a metadata key then only admit our message inter-event msg_type_list
                return;
            }
            msg_iev_write_key(kw, "__msg_type__", msg_type);
        }
    }

    /************************************************************
     *
     ************************************************************/
    function msg_get_msg_type(kw)
    {
        return msg_iev_read_key(kw, "__msg_type__");
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
    function search_delimiter(s, delimiter_)
    {
        if(!delimiter_) {
            return 0;
        }
        return strchr(s, delimiter_);
    }

    /************************************************************
     *
     ************************************************************/
    function _kw_search_path(kw, path)
    {
        if(!is_object(kw)) {
            // silence
            return 0;
        }
        if(!is_string(path)) {
            log_error("path must be a string: " + String(path));
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
                return undefined;
            }
        }
        return kw;
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_bool(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value?true:false;
        }
        let b = _kw_search_path(kw, key);
        if(b === undefined) {
            if(create) {
                kw[key] = default_value?true:false;
            } else if(verbose) {
                log_error("kw_get_bool() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value?true:false;
        }
        return b?true:false;
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_int(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        let v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_int() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }

        if(!is_number(v)) {
            if(verbose) {
                log_error("path value MUST BE a number: " + key);
                trace_msg(kw);
            }
            return default_value;
        }

        return parseInt(v);
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_real(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        let v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_real() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }

        if(!is_number(v)) {
            if(verbose) {
                log_error("path value MUST BE a number: " + key);
                trace_msg(kw);
            }
            return default_value;
        }

        return parseFloat(v);
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_str(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        let v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_str() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }

        if(!is_string(v)) {
            if(verbose) {
                log_error("path value MUST BE a string: " + key);
                trace_msg(kw);
            }
            return default_value;
        }

        return String(v);
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_dict(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        let v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_dict() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }
        if(!is_object(v)) {
            if(verbose) {
                log_error("path value MUST BE a json dict: " + key);
                trace_msg(kw);
            }
            return default_value;
        }

        return v;
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_list(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        let v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_list() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }
        if(!is_array(v)) {
            if(verbose) {
                log_error("path value MUST BE a json list: " + key);
                trace_msg(kw);
            }
            return default_value;
        }

        return v;
    }

    /************************************************************
     *
     ************************************************************/
    function kw_get_dict_value(kw, key, default_value, create, verbose)
    {
        if(kw !== Object(kw)) {
            return default_value;
        }
        var v = _kw_search_path(kw, key);
        if(v === undefined) {
            if(create) {
                kw[key] = default_value;
            } else if(verbose) {
                log_error("kw_get_dict_value() path not found: '" + key + "'");
                trace_msg(kw);
            }
            return default_value;
        }
        return v;
    }

    /************************************************************
     *  TODO implement path,
     *  TODO and change key by path in all kw_get_...() functions
     ************************************************************/
    function kw_set_dict_value(kw, path, value)
    {
        if(!is_object(kw)) {
            log_error("kw is not an object");
            return -1;
        }

        kw[path] = value;
        return 0;
    }


    /************************************************************
     *  TODO implement path,
     ************************************************************/
    function kw_set_subdict_value(kw, path, key, value)
    {
        if(!is_object(kw)) {
            log_error("kw is not an object");
            return -1;
        }

        let path_ = kw[path];
        if(is_null(path_) || !is_object(path_)) {
            path_ = {};
            kw[path] = path_;
        }
        path_[key] = value;
        return 0;
    }

    /************************************************************
     *  Return object with private data (HACK original kw modified)
     *  (keys begin with "_" are extracted, removed from source)
     ************************************************************/
    function kw_extract_private(kw)
    {
        var copy = {};

        for (var attr in kw) {
            if (kw.hasOwnProperty(attr)) {
                if(is_private_key(attr)) {
                    copy[attr] = kw[attr];
                    delete kw[attr];
                }
            }
        }

        return copy;
    }

    /************************************************************
     *
     ************************************************************/
    function get_unique_id(prefix)
    {
        if(!prefix) {
            prefix = "random";
        }
        return prefix + "-" + uuidv4();
    }

    /************************************************************
     *  https://stackoverflow.com/questions/105034/create-guid-uuid-in-javascript#2117523
     ************************************************************/
    function uuidv4()
    {
        return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g,
            function(c) {
                var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
                return v.toString(16);
            }
        );
    }

    /************************************************************
     *          Load json file from server
     ************************************************************/
    function fileLoaded(xhr) {
        return xhr.status == 0 && xhr.responseText && xhr.responseURL.startsWith("file:");
    }
    function load_json_file(url, on_success, on_error)
    {
        let req = new XMLHttpRequest();
        req.open("GET", url, true);
        req.setRequestHeader("Accept", "application/json");

        req.onreadystatechange = function () {
            if (req.readyState == 4) {
                if (req.status == 200 || fileLoaded(req)) {
                    let json = JSON.parse(req.responseText);
                    on_success(json);
                } else {
                    if(on_error) {
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
            if (xhr.readyState == 4) {
                on_response(xhr.status, xhr.responseText);
            }
        };
        xhr.send(data);
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
        if (pos == -1) {
            return false;
        } else {
            if (bool) {
                return haystack.substring(0, pos);
            } else {
                return haystack.slice(pos);
            }
        }
    }

    /********************************************************
     *  Convert [s] or [{}] or {}
     *  in a webix list options:
     *      [{id:"", value:""}, ...]
     ********************************************************/
    function list2options(list, field_id, field_value)
    {
        field_id = field_id?field_id:"id";
        field_value = field_value?field_value:"value";

        let options = [];

        if(is_array(list)) {
            for(let i=0; i<list.length; i++) {
                let v = list[i];
                if(is_string(v)) {
                    options.push({
                        id: list[i],
                        value: list[i]
                    });
                } else if(is_object(v)) {
                    let vv = {};
                    if(!kw_has_key(v, field_id)) {
                        log_error("list2options(): object without field id: " + field_id);
                    }
                    if(!kw_has_key(v, field_value)) {
                        log_error("list2options(): object without field value: " + field_value);
                    }
                    vv["id"] = v[field_id];
                    vv["value"] = v[field_value];
                    options.push(vv);
                } else {
                    log_error("list2options(): case1 not implemented");
                }
            }
        } else if(is_object(list)) {
            for(let k in list) {
                options.push({
                    id: k,
                    value: k
                });
            }
        } else {
            log_error("list2options(): case2 not implemented");
        }

        return options;
    }

    /************************************************************
     *  callback (obj, key, value, full_path)
     ************************************************************/
    function traverse_dict(obj, callback, full_path)
    {
        if(full_path == undefined || !is_string(full_path)) {
            full_path = "";
        }
        for (var key in obj) {
            if(!obj.hasOwnProperty(key)) {
                continue;
            }
            var sufix = (full_path.length? "`":"") + key;
            full_path += sufix;

            callback.apply(this, [obj, key, obj[key], full_path]);

            if(is_object(obj[key])) {
                //going one step down in the object tree!!
                traverse_dict(obj[key], callback, full_path);
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
            jdb = __duplicate__(jdb);
        }
        var type = kw_get_dict_value(jdb, "type", [], 1);
        var hook = kw_get_str(jdb, "hook", "data", 1);
        var schema = kw_get_dict_value(jdb, "schema", {}, 1);
        var topics = kw_get_dict_value(jdb, "topics", {}, 1);

        // Create topics defined in schema
        var walk = function(obj, key, value, full_path) {
            if(key.substring(0, 2) != "__") {
                kw_get_dict_value(topics, full_path, __duplicate__(type), 1);
                //trace_msg(sprintf("full_path: '%s', key: %s, value: %j", full_path, key, value));
            }
        };
        traverse_dict(schema, walk, prefix);

        return jdb;
    }

    /********************************************
     *
     ********************************************/
    function jdb_update(jdb, topic_name, path, kw)
    {
        var topics = kw_get_dict_value(jdb, "topics", null, 0);
        var topic = kw_get_dict_value(topics, topic_name, null, 0);
        if(!topic) {
            log_error("jdb_update: topic not found: " + topic_name);
            return null;
        }
        if(empty_string(kw["id"])) {
            log_error("jdb_update: record without id: " + kw);
            trace_msg(kw);
            return null;
        }

        var ids = path.split("`");

        var id;
        var v = topic;
        while((id = ids.shift())) {
            if(empty_string(id) || !v) {
                break;
            }
            if(is_object(v)) {
                v = kw_get_dict_value(v, jdb.hook, [], 1);
            }
            v = _jdb_get(v, null, id, false);
        }

        if(ids.length==0 && v) {
            if(is_array(v)) {
                v.push(kw);
            } else {
                if(v["id"]==kw["id"]) {
                    Object.assign(v, kw);
                } else {
                    var v_ = kw_get_dict_value(v, jdb.hook, [], 1);
                    var v__ = _jdb_get(v_, jdb.hook, kw["id"], false);
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
        var topics = kw_get_dict_value(jdb, "topics", null, 0);
        var topic = kw_get_dict_value(topics, topic_name, null, 0);
        if(!topic) {
            log_error("jdb_delete: topic not found: " + topic_name);
            return null;
        }
        if(empty_string(kw["id"])) {
            log_error("jdb_delete: record without id: " + kw);
            trace_msg(kw);
            return null;
        }

        var ids = path.split("`");

        var id;
        var v = topic;
        while((id = ids.shift())) {
            if(empty_string(id) || !v) {
                break;
            }
            if(is_object(v)) {
                v = kw_get_dict_value(v, jdb.hook, null, 0);
            }
            v = _jdb_get(v, null, id, false);
        }

        if(ids.length==0 && v) {
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
        var topics = kw_get_dict_value(jdb, "topics", null, 0, false);
        if(!topics) {
            log_error("jdb topics section not found");
            trace_msg(jdb);
            return null;
        }
        var topic = kw_get_dict_value(topics, topic_name, null, 0, false);
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
        var topics = kw_get_dict_value(jdb, "topics", null, 0);
        var topic = kw_get_dict_value(topics, topic_name, null, 0);
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
        var topics = kw_get_dict_value(jdb, "topics", null, 0);
        var topic = kw_get_dict_value(topics, topic_name, null, 0);
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
        var j;
        var ln = v.length;
        for(j=0; j<ln; j++) {
            var v_ = v[j];
            if(v_) {
                var id_ = v_["id"];
                if(id_ && (id_ == id)) {
                    return v_;
                }
                if(recursive) {
                    if(kw_has_key(v_, hook)) {
                        var v__ = _jdb_get(v_[hook], hook, id, recursive);
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
        let record = __duplicate__(json_desc);  // Get fields and default vaues from json_desc
        json_object_update_existing(record, value); // Update (only with service fields) with user data

        return record;
    }

    /************************************************************
     *          log function
     ************************************************************/

    /************************************************************
     *          low level log function
     ************************************************************/
    function _logger(msg)
    {
        console.log(msg);
    }

    /********************************************
     *  Log functions
     ********************************************/
    var f_error = null;
    var f_warning = null;
    var f_info = null;
    var f_debug = null;

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
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
        let hora = get_current_datetime();
        console.log("%c" + hora + " ERROR: " + String(msg), "color:yellow");

        if(f_error) {
            f_error("" + hora + " ERROR: " + String(msg));
        }

        try {
            // Code throwing an exception
            throw new Error();
        } catch(e) {
            console.log(e.stack);
        }
    }

    function log_warning(msg)
    {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
        let hora = get_current_datetime();
        console.log("%c" + hora + " WARNING: " + String(msg), "color:cyan");
        if(f_warning) {
            f_warning("" + hora + " WARNING: " + String(msg));
        }
    }

    function log_info(msg)
    {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
        if(!empty_string(msg)) {
            console.log(String(msg));
        }
        if(f_info) {
            f_info(String(msg));
        }
    }

    function log_debug(msg)
    {
        if(is_object(msg)) {
            msg = JSON.stringify(msg);
        }
        if(!empty_string(msg)) {
            console.log(String(msg));
        }
        if(f_debug) {
            f_debug(String(msg));
        }
    }

    function trace_msg(msg)
    {
        console.log(msg);
        if(f_debug) {
            f_debug(String(msg));
        }
    }

    function trace_msg2(msg, msg2)
    {
        console.log(msg);
        console.log(msg2);
        if(f_debug) {
            f_debug(String(msg));
            f_debug(String(msg2));
        }
    }

    /********************************************
     *  Get a local attribute
     ********************************************/
    function kw_get_local_storage_value(key, default_value, create)
    {
        if(!(key && window.JSON && window.localStorage)) {
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
        if(key && window.JSON && window.localStorage) {
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
        if(key && window.localStorage) {
            window.localStorage.removeItem(key);
        }
    }

    /*
     * Converts a string to a bool.
     *
     * This conversion will:
     *
     *  - match 'true', 'on', or '1' as true.
     *  - ignore all white-space padding
     *  - ignore capitalization (case).
     *
     * '  tRue  ','ON', and '1   ' will all evaluate as true.
     *
     */
    function parseBoolean(s)
    {
        if(is_number(s)) {
            return Boolean(s);
        }

        // will match one and only one of the string 'true','1', or 'on' rerardless
        // of capitalization and regardless off surrounding white-space.
        //
        let regex=/^\s*(true|1|on)\s*$/i

        return regex.test(s);
    }

    /********************************************
     *
     ********************************************/
    function escapeRegExp(stringToGoIntoTheRegex) {
        return stringToGoIntoTheRegex.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
    }

    /********************************************
     *  Code copied from
     *  https://stackoverflow.com/questions/17885855/use-dynamic-variable-string-as-regex-pattern-in-javascript
     *
     *  Example of use:
     *
     *      var regex = replace_variable_engine("ip");
     *      var input = "wss://#ip#:1600";
     *      var output = input.replace(regex, "localhost");
     *
     *          output: "wss://localhost:1600"
     *
     ********************************************/
    function replace_variable_engine(variable)
    {
        var variable2 = escapeRegExp(variable);
        return new RegExp("#" + variable2 + "#", "g");
    }

    /********************************************
     *
     ********************************************/
    function zot(v) {
        return v==null; // both null and undefined match but not false or 0
    }

    /********************************************
     *
     ********************************************/
    /*--
    zim.Proportion = function(baseMin, baseMax, targetMin, targetMax, factor, targetRound, clamp)

    Proportion
    zim class

    DESCRIPTION
    Proportion converts an input value to an output value on a different scale.
    (sometimes called a map() function)
    For instance, like a slider controlling the scale of an object or sound volume.
    Make a Proportion object and then in an interval, ticker or event,
    convert the base value to the target value using the convert method.

    NOTE: as of ZIM 5.5.0 the zim namespace is no longer required (unless zns is set to true before running zim)

    EXAMPLE
    frame.loadAssets("mySound.mp3");
    frame.on("complete", function() {
        var sound = frame.asset("mySound.mp3").play();
        var p = new Proportion(0, 10, 0, 1);
        var dial = new Dial(); // default range of 0 to 10
        dial.currentValue = 10;
        dial.on("change", function(){
            sound.volume = p.convert(dial.currentValue);
        }); // end of dial change
    }); // end sound loaded
    END EXAMPLE

    PARAMETERS supports DUO - parameters or single object with properties below
    baseMin - min for the input scale (say x value)
    baseMax - max for the input scale (say x value)
    targetMin - (default 0) min for the output scale (say volume)
    targetMax - (default 1) max for the output scale (say volume)
    factor - (default 1) is going the same direction and -1 is going in opposite direction
    targetRound - (default false) set to true to round the converted number
    clamp - (default true) set to false to let results go outside min and max range

    METHODS
    convert(input) - will return the output property (for instance, a volume)

    NOTE: the object always starts by assuming baseMin as baseValue
    just call the convert method right away if you want it to start at a different baseValue
    for instance, if your slider went from 100 to 500 and you want to start at half way
    make the object and call p.convert(300); on the next line
    --*/
    let Proportion = function(baseMin, baseMax, targetMin, targetMax, factor, targetRound, clamp) {
        // factor - set to 1 for increasing and -1 for decreasing
        // round - true to round results to whole number
        if (zot(targetMin)) targetMin = 0;
        if (zot(targetMax)) targetMax = 1;
        if (zot(factor)) factor = 1;
        if (zot(targetRound)) targetRound = false;

        // proportion
        var proportion;
        var targetAmount;

        this.convert = function(baseAmount) {
            if (zot(baseAmount)) {
                baseAmount = baseMin; // just start at the min otherwise call immediate(baseValue);
            }
            if (isNaN(baseAmount) || (baseMax-baseMin==0)) {
                return;
            }
            if (clamp) {
                baseAmount = Math.max(baseAmount, baseMin);
                baseAmount = Math.min(baseAmount, baseMax);
            }
            proportion = (baseAmount - baseMin) / (baseMax - baseMin);
            if (factor > 0) {
                targetAmount = targetMin + (targetMax-targetMin) * proportion;
            } else {
                targetAmount = targetMax - (targetMax-targetMin) * proportion;
            }
            if (targetRound) {targetAmount = Math.round(targetAmount);}
            return targetAmount;
        };
    };

    /************************************************
     *  Adjust font size to screen's pixels
     *  to see the same size in mobiles with text size changed
     ************************************************/
    function adjust_font_size(wanted_size, fontFamily)
    {
        let h;

        for(h = 1; h <80; h++) {
            let dim = get_text_size("MWj|}", fontFamily, h, 0);
            if(dim.height >= wanted_size) {
                break;
            }
        }
        return h;
    }

    /************************************************
     *  Adjust font size to screen's pixels
     *  to see the same size in mobiles with text size changed
     ************************************************/
    function adjust_icon_size(wanted_size, fontFamily)
    {
        let h;

        for(h = 1; h <80; h++) {
            let dim = get_text_size("\u{EA01}\u{EA02}", fontFamily, h, 0);
            if(dim.height >= wanted_size) {
                break;
            }
        }
        return h;
    }

    /********************************************
     *
     ********************************************/
    function get_text_size(text, font_family, font_size, padding)
    {
        let pa = document.body;
        let who = document.createElement('div');

        if(is_null(padding)) {
            padding = 0;
        }
        if(is_number(padding)) {
            padding = padding + "px";
        }
        if(is_number(font_size)) {
            font_size = font_size + "px";
        }
        if(empty_string(font_size)) {
            font_size = "1em";
        }
        if(empty_string(font_family)) {
            font_family = "sans-serif";
        }
        if(empty_string(text)) {
            text = "Mj";
        }
        who.style.cssText="display:inline-block;padding:" + padding + ";line-height:1;position:absolute;visibility:hidden font-family:"+ font_family + ";font-size:" + font_size + ";";

        who.appendChild(document.createTextNode(text));
        pa.appendChild(who);
        //let fs= {width: who.offsetWidth, height: who.offsetHeight};
        let height = who.offsetHeight;
        let width = who.offsetWidth;
        pa.removeChild(who);

        return {width: width, height: height};
    }

    /********************************************
     *
     ********************************************/
    function htmlToElement(html)
    {
        let template = document.createElement('template');
        html = html.trim(); // Never return a text node of whitespace as the result
        template.innerHTML = html;
        return template.content.firstChild;
    }

    /************************************************************
     *        Get current date
     *        Return string in "2022/09/12 12:27:15" format
     ************************************************************/
    function get_current_datetime()
    {
        let currentTime = new Date();
        let month = currentTime.getMonth() + 1;
        if (month < 10) {
            month = "0" + month;
        }
        let day = currentTime.getDate();
        if (day < 10) {
            day = "0" + day;
        }
        let year = currentTime.getFullYear();
        let fecha = year + "/" + month + "/" + day;

        let hours = currentTime.getHours();
        if (hours < 10) {
            hours = "0" + hours;
        }
        let minutes = currentTime.getMinutes();
        if (minutes < 10) {
            minutes = "0" + minutes;
        }
        let seconds = currentTime.getSeconds();
        if (seconds < 10) {
            seconds = "0" + seconds;
        }
        let hora = hours + ":" + minutes + ":" + seconds;
        return fecha + " " + hora;
    }

    /********************************************
     *  Return UTC time of right now
     ********************************************/
    function get_now()
    {
        let start = new Date();
        start.setMilliseconds(0);

        return start/1000;
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
            let jwt_decoded = jwtDecode(jwt)
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

    /***************************************************************************
     *
     ***************************************************************************/
    function icono(name)
    {
        let icon_unicode = yuneta_icon_font[name];
        if(!icon_unicode) {
            log_error("Yuneta icon not found: '" + name + "'");
            return null;
        }
        return String.fromCharCode(parseInt(icon_unicode, 10))
    }

    /***************************************************************************
     *
     ***************************************************************************/
    function find_gobj_in_list(list, name) // TODO elimina, usa gobj api
    {
        for(let gobj of list) {
            if(!is_gobj(gobj)) {
                continue;
            }
            if(gobj.gobj_name() === name) {
                return gobj;
            }
        }
        return null;
    }

    /***************************************************************************
     *
     ***************************************************************************/
    function get_str_list_difference(array1, array2)
    {
        let difference1 = [];
        for (let i = 0; i < array1.length; i++) {
            if (!array2.includes(array1[i])) {
                difference1.push(array1[i]);
            }
        }

        let difference2 = [];
        for (let i = 0; i < array2.length; i++) {
            if (!array1.includes(array2[i])) {
                difference2.push(array2[i]);
            }
        }

        return {
            array1: difference1,
            array2: difference2
        };
    }

    /***************************************************************************
     *  Get the dirName of window.location.pathname
     ***************************************************************************/
    function get_location_path_root() {
        /*
         *  window.location.pathname
         *  en browser  = "/"
         *  en electron = "/yuneta/development/.../gui_yunetas.js/tags/0.00.aa/index.html"
         *  en android  = "/index.html"
         */
        // Split the path by '/' and get the last component
        const segments = window.location.pathname.split('/');
        const baseName = segments.pop(); // remove basename

        // Join the remaining segments back into a string
        return segments.join('/');
    }

    /***************************************************************************
     *  implement a debounce or throttle mechanism to limit the number
     *  of times the callback function is executed
     ***************************************************************************/
    function debounce(func, wait)
    {
        let timeout;
        return function executedFunction() {
            const later = () => {
               clearTimeout(timeout);
                func();
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
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
     *                          content = i18next.t(attrs['i18n']));
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
    function createElement2(description) {
        let [tag, attrs, content, events] = description;

        /*
         *  Create the html element
         */
        if (!tag) {
            console.error("Tag must be a valid HTML element.");
            return;
        }
        let el = document.createElement(tag);

        /*
         *  Append attributes
         */
        let data_i18next = '';

        if(attrs && typeof attrs === 'object') {
            for (let attr in attrs) {
                if (attr === 'style' && typeof attrs[attr] === 'object') {
                    // Convert the style object to a CSS string
                    let styleString = '';
                    for (let prop in attrs[attr]) {
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
        if (typeof content === 'string') {
            content = content.trim();
            if(content.length > 0 && content[0] === '<') {
                el.appendChild(createOneHtml(content));
            } else {
                if(data_i18next) {
                    el.textContent = i18next.t(data_i18next, content);
                } else {
                    el.textContent = content;
                }
            }
        } else if (Array.isArray(content) && content.length > 0) {
            if(is_string(content[0])) {
                el.appendChild(createElement2(content));
            } else {
                for (let child of content) {
                    // Check if the child is an array description or an HTMLElement
                    if (Array.isArray(child) && child.length > 0) {
                        el.appendChild(createElement2(child));
                    } else if (child instanceof HTMLElement) {
                        el.appendChild(child);
                    }
                }
            }
        } else if (content instanceof HTMLElement || content instanceof Text) {
            el.appendChild(content);
        }

        /*
         *  Attach event listeners
         */
        if (events && typeof events === 'object') {
            for (let eventType in events) {
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
     *  Function to convert dataset (of DOM Element) to plain object
     ***************************************************************************/
    function datasetToObject(dataset)
    {
        return Object.fromEntries(
            Object.entries(dataset).map(([key, value]) => [key, value])
        );
    }

    /************************************************************
     *   Build name
     ************************************************************/
    function build_name(self, name)
    {
        // We need unique names
        if(empty_string(self.gobj_name())) {
            if(!self._uuid_name) {
                self._uuid_name = get_unique_id(self.gobj_gclass_name());
            }
            return self._uuid_name + "-" + name;
        }
        return self.gobj_escaped_short_name() + "-" + name;
    }

    /************************************************************
     *   Build gobj clean name
     ************************************************************/
    function build_clean_gobj_name(self, name)
    {
        let clean_name = self.gobj_short_name().replace(/[?# ^:]/g, '_');
        return name + "-" + clean_name;
    }

    /************************************************************
     *   Build clean name
     ************************************************************/
    function clean_name(name)
    {
        return name.replace(/[?# ^:]/g, '_');
    }

    /************************************************************
     *  Get the first gobj parent matching a gclass
     ************************************************************/
    function gobj_near_parent(gobj, gclass_name) // TODO put in gobj.c gobj.js
    {
        while (gobj) {
            gobj = gobj.gobj_parent();
            if (gobj) {
                // Check if the element has defined width and height
                if(gobj.gobj_gclass_name() === gclass_name) {
                    return gobj;
                }
            }
        }
        return null; // No parent found
    }

    /************************************************************
     *  Get the first html parent matching a CSS selector
     *  - 'selector' is same parameter used by querySelector()
     ************************************************************/
    function element_near_parent($element, selector)
    {
        while ($element) {
            if ($element.matches && $element.matches(selector)) {
                return $element;
            }
            $element = $element.parentElement;
        }
        return null; // Return null if no parent matching the selector is found
    }


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.__duplicate__ = __duplicate__;
    exports.__update_dict__ = __update_dict__;
    exports.json_object_update_existing_recursive = json_object_update_existing_recursive;
    exports.json_object_update_existing = __update_dict__;
    exports.__extend_dict__ = __extend_dict__;
    exports.json_object_update = __extend_dict__;
    exports.__extend_list__ = __extend_list__;
    exports.json_object_update_missing = json_object_update_missing;
    exports.array_real_length = array_real_length;
    exports.index_of_list = index_of_list;
    exports.elm_in_list = elm_in_list;
    exports.elms_in_list = elms_in_list;
    exports.index_in_list = index_in_list;
    exports.delete_from_list = delete_from_list;
    exports.__strip__ = __strip__;
    exports.__set__ = __set__;
    exports.get_function_name = get_function_name;
    exports.same_list = same_list;
    exports.none_in_list = none_in_list;
    exports.id_index_in_obj_list = id_index_in_obj_list;
    exports.get_object_from_list = get_object_from_list;
    exports.strncmp = strncmp;
    exports.is_object = is_object;
    exports.json_object_size = json_object_size;
    exports.json_size = json_size;
    exports.is_array = is_array;
    exports.is_string = is_string;
    exports.is_number = is_number;
    exports.is_boolean = is_boolean;
    exports.is_null = is_null;
    exports.is_date = is_date;
    exports.is_function = is_function;
    exports.is_gobj = is_gobj;
    exports.empty_string = empty_string;
    exports.kw_is_identical = kw_is_identical;
    exports.strcmp = strcmp;
    exports.cmp_two_simple_json = cmp_two_simple_json;
    exports.kw_match_simple = kw_match_simple;
    exports.kw_collect = kw_collect;
    exports.kwid_get_ids = kwid_get_ids;
    exports.kwid_match_id = kwid_match_id;
    exports.kwid_collect = kwid_collect;
    exports.kwid_new_dict = kwid_new_dict;
    exports.kwid_get_first_id = kwid_get_first_id;
    exports.kwid_find_one_record = kwid_find_one_record;
    exports.kwid_delete_record = kwid_delete_record;
    exports.match_dict_list_by_kw = match_dict_list_by_kw;
    exports.filter_dictlist = filter_dictlist;
    exports.filter_dict = filter_dict;
    exports.filter_list = filter_list;
    exports.ids2str = ids2str;

    exports.msg_iev_read_key = msg_iev_read_key;
    exports.msg_iev_write_key = msg_iev_write_key;
    exports.msg_iev_add_answer_filter = msg_iev_add_answer_filter;
    exports.msg_iev_answer = msg_iev_answer;
    exports.msg_iev_push_stack = msg_iev_push_stack;
    exports.msg_iev_get_stack = msg_iev_get_stack;
    exports.msg_iev_pop_stack = msg_iev_pop_stack;
    exports.msg_set_msg_type = msg_set_msg_type;
    exports.msg_get_msg_type = msg_get_msg_type;
    exports.kw_has_key = kw_has_key;
    exports.kw_get_bool = kw_get_bool;
    exports.kw_get_int = kw_get_int;
    exports.kw_get_real = kw_get_real;
    exports.kw_get_str = kw_get_str;
    exports.kw_get_dict = kw_get_dict;
    exports.kw_get_list = kw_get_list;
    exports.kw_get_dict_value = kw_get_dict_value;
    exports.kw_set_dict_value = kw_set_dict_value;
    exports.kw_set_subdict_value = kw_set_subdict_value;
    exports.kw_extract_private = kw_extract_private;
    exports.get_unique_id = get_unique_id;
    exports.uuidv4 = uuidv4;
    exports.load_json_file = load_json_file;
    exports.send_http_json_post = send_http_json_post;
    exports.strstr = strstr;
    exports.list2options = list2options;

    exports.traverse_dict = traverse_dict;
    exports.jdb_init = jdb_init;
    exports.jdb_update = jdb_update;
    exports.jdb_delete = jdb_delete;
    exports.jdb_get_topic = jdb_get_topic;
    exports.jdb_get = jdb_get;
    exports.jdb_get_by_idx = jdb_get_by_idx;
    exports.create_json_record = create_json_record;

    exports._logger = _logger;
    exports.set_log_functions = set_log_functions;
    exports.log_error = log_error;
    exports.log_warning = log_warning;
    exports.log_info = log_info;
    exports.log_debug = log_debug;
    exports.trace_msg = trace_msg;
    exports.trace_msg2 = trace_msg2;

    exports.kw_get_local_storage_value = kw_get_local_storage_value;
    exports.kw_set_local_storage_value = kw_set_local_storage_value;
    exports.kw_remove_local_storage_value = kw_remove_local_storage_value;
    exports.parseBoolean = parseBoolean;
    exports.escapeRegExp = escapeRegExp;
    exports.replace_variable_engine = replace_variable_engine;
    exports.Proportion = Proportion;
    exports.get_text_size = get_text_size;
    exports.adjust_font_size = adjust_font_size;
    exports.adjust_icon_size = adjust_icon_size;
    exports.htmlToElement = htmlToElement;
    exports.get_current_datetime = get_current_datetime;
    exports.get_now = get_now;
    exports.jwtDecode = jwtDecode;
    exports.jwt2json = jwt2json;
    exports.is_metadata_key = is_metadata_key;
    exports.is_private_key = is_private_key;
    exports.icono = icono;
    exports.find_gobj_in_list = find_gobj_in_list; // TODO elimina, usa gobj api
    exports.get_str_list_difference = get_str_list_difference;
    exports.get_location_path_root = get_location_path_root;
    exports.debounce = debounce;
    exports.createOneHtml = createOneHtml;
    exports.createElement2 = createElement2;
    exports.getPositionRelativeToBody = getPositionRelativeToBody;
    exports.datasetToObject = datasetToObject;
    exports.build_name = build_name;
    exports.build_clean_gobj_name = build_clean_gobj_name;
    exports.clean_name = clean_name;
    exports.gobj_near_parent = gobj_near_parent;
    exports.element_near_parent = element_near_parent;
})(this);
