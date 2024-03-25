/***********************************************************************
 *          jsoneditor.js
 *
 *          Load modules to be used in non-module environment:

 *          Copyright (c) 2022, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/******************************************************************
 *  JSONEditor
 *  WARNING: this works in browser, but not with Apache Cordova
 *  It's better put this code in index.mako

        <script type="module">
            import { JSONEditor} from "./static/jsoneditor/standalone.js";
            if(JSONEditor) {
                window.JSONEditor = JSONEditor;
            }
        </script>

 ******************************************************************/
import { JSONEditor} from "/static/jsoneditor/standalone.js";
window.JSONEditor = JSONEditor;
