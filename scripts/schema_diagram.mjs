#!/usr/bin/env node
/*
 *
 *   schema_diagram.mjs -- head each treedb_schema_<db>.c with the graph of its schema
 *
 *   A `treedb_schema_<db>.c` holds two things, both written by the schema
 *   editor's export (schema_to_c() in gobj-ui): the graph of the schema as
 *   a comment, and the literal. This script writes the first half from the
 *   second one, with the same code the editor uses, so a file kept in the
 *   repo is what an export of it would say.
 *
 *   The literal is left byte for byte as it is; only what comes before it
 *   is replaced. Before writing, the literal is put through the exporter
 *   (json_to_c) and read back: a schema the export would not reproduce is
 *   reported and the file is left alone.
 *
 *   Usage:
 *       scripts/schema_diagram.mjs FILE...           rewrite the comment
 *       scripts/schema_diagram.mjs --check FILE...   exit 1 if one is stale
 *
 */
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { isDeepStrictEqual } from "node:util";

const HERE = dirname(fileURLToPath(import.meta.url));
const GOBJ_UI = join(HERE, "..", "kernel", "js", "gobj-ui", "src");
const { json_to_c } = await import(join(GOBJ_UI, "schema_to_c.js"));

/*
 *  The literal as the yuno reads it: the C compiler's continuations and
 *  escapes, then helper_quote2doublequote() on every single quote.
 */
function literal_to_json(source)
{
    const start = source.indexOf('"\\\n');
    const end = source.lastIndexOf('";');
    if(start < 0 || end < start) {
        return null;
    }
    let text = source.slice(start + 3, end).split("\\n\\\n").join("\n");
    text = text.replace(/\\n\\$/, "");
    let out = "";
    for(let i = 0; i < text.length; i++) {
        if(text[i] === "\\" && i + 1 < text.length && (text[i + 1] === "\\" || text[i + 1] === '"')) {
            out += text[i + 1];
            i++;
            continue;
        }
        out += text[i];
    }
    return JSON.parse(out.split("'").join('"'));
}

/*
 *  Where the literal starts: the `static char <name>[]` line.
 */
function literal_start(source)
{
    const m = source.match(/^static char ([A-Za-z0-9_]+)\[\]\s*=\s*"\\$/m);
    return m ? {index: m.index, name: m[1]} : null;
}

let check = false;
let files = [];
for(const arg of process.argv.slice(2)) {
    if(arg === "--check") {
        check = true;
    } else {
        files.push(arg);
    }
}
if(files.length === 0) {
    console.error("usage: schema_diagram.mjs [--check] treedb_schema_<db>.c...");
    process.exit(2);
}

let failed = 0;
for(const file of files) {
    const source = readFileSync(file, "utf8");
    const at = literal_start(source);
    if(!at) {
        console.error(`${file}: no 'static char <name>[]= "\\' literal`);
        failed++;
        continue;
    }
    const literal = source.slice(at.index);
    let json;
    try {
        json = literal_to_json(literal);
    } catch(e) {
        console.error(`${file}: the literal does not parse: ${e.message}`);
        failed++;
        continue;
    }

    /*  The export must give this schema back, or the file could not be
     *  replaced by one.  */
    const exported = json_to_c(json, {var_name: at.name});
    const again = literal_to_json(exported.slice(literal_start(exported).index));
    if(!isDeepStrictEqual(again, json)) {
        console.error(`${file}: the exporter does not reproduce this schema; left alone`);
        failed++;
        continue;
    }

    const header = exported.slice(0, literal_start(exported).index);
    const wanted = header + literal;
    if(wanted === source) {
        continue;
    }
    if(check) {
        console.error(`${file}: the diagram is not the one its literal draws`);
        failed++;
        continue;
    }
    writeFileSync(file, wanted);
    console.log(`${file}: diagram written`);
}
process.exit(failed ? 1 : 0);
