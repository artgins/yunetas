import { defineConfig } from "vite";
import terser from "@rollup/plugin-terser";

export default defineConfig({
    build: {
        lib: {
            entry: "index.js",
            name: "libYui"
        },
        rollupOptions: {
            external: [
                "@yuneta/gobj-js",
                "@antv/g6",
                "bulma",
                "i18next",
                "maplibre-gl",
                "tabulator-tables",
                "tom-select",
                "uplot",
                "vanilla-jsoneditor",
            ],
            output: [
                // Non-minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "lib-yui.es.js",
                    compact: false
                },
                // Non-minified UMD Module
                {
                    name: "libYui",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "lib-yui.umd.js",
                    compact: false,
                    globals: {
                        "@yuneta/gobj-js": "gobjJs",
                        "@antv/g6": "G6",
                        "i18next": "i18next",
                        "maplibre-gl": "maplibregl",
                        "tabulator-tables": "Tabulator",
                        "tom-select": "TomSelect",
                        "uplot": "uPlot",
                        "vanilla-jsoneditor": "JSONEditor",
                    }
                },
                // Non-minified CJS Module
                {
                    name: "libYui",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "lib-yui.cjs.js",
                    compact: false
                },
                // Non-minified IIFE Module
                {
                    name: "libYui",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "lib-yui.iife.js",
                    compact: false,
                    globals: {
                        "@yuneta/gobj-js": "gobjJs",
                        "@antv/g6": "G6",
                        "i18next": "i18next",
                        "maplibre-gl": "maplibregl",
                        "tabulator-tables": "Tabulator",
                        "tom-select": "TomSelect",
                        "uplot": "uPlot",
                        "vanilla-jsoneditor": "JSONEditor",
                    }
                },

                // Minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "lib-yui.es.min.js",
                    plugins: [terser()]
                },
                // Minified UMD Module
                {
                    name: "libYui",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "lib-yui.umd.min.js",
                    plugins: [terser()],
                    globals: {
                        "@yuneta/gobj-js": "gobjJs",
                        "@antv/g6": "G6",
                        "i18next": "i18next",
                        "maplibre-gl": "maplibregl",
                        "tabulator-tables": "Tabulator",
                        "tom-select": "TomSelect",
                        "uplot": "uPlot",
                        "vanilla-jsoneditor": "JSONEditor",
                    }
                },
                // Minified CJS Module
                {
                    name: "libYui",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "lib-yui.cjs.min.js",
                    plugins: [terser()]
                },
                // Minified IIFE Module
                {
                    name: "libYui",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "lib-yui.iife.min.js",
                    plugins: [terser()],
                    globals: {
                        "@yuneta/gobj-js": "gobjJs",
                        "@antv/g6": "G6",
                        "i18next": "i18next",
                        "maplibre-gl": "maplibregl",
                        "tabulator-tables": "Tabulator",
                        "tom-select": "TomSelect",
                        "uplot": "uPlot",
                        "vanilla-jsoneditor": "JSONEditor",
                    }
                }
            ]
        },
        sourcemap: true,
        minify: false,
        target: "esnext"
    }
});
