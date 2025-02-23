import { defineConfig } from "vite";
import terser from "@rollup/plugin-terser";

export default defineConfig({
    build: {
        lib: {
            entry: "src/index.js",
            name: "yunetas"
        },
        rollupOptions: {
            output: [
                // Non-minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "yunetas.es.js",
                    compact: false // ✅ Keep original formatting
                },
                // Non-minified UMD Module
                {
                    name: "yunetas",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "yunetas.umd.js",
                    compact: false
                },
                // Non-minified CJS Module
                {
                    name: "yunetas",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "yunetas.cjs.js",
                    compact: false
                },
                // Non-minified IIFE Module
                {
                    name: "yunetas",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "yunetas.iife.js",
                    compact: false
                },

                // Minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "yunetas.es.min.js",
                    plugins: [terser()] // ✅ Minified version
                },
                // Minified UMD Module
                {
                    name: "yunetas",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "yunetas.umd.min.js",
                    plugins: [terser()]
                },
                // Minified CJS Module
                {
                    name: "yunetas",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "yunetas.cjs.min.js",
                    plugins: [terser()]
                },
                // Minified IIFE Module
                {
                    name: "yunetas",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "yunetas.iife.min.js",
                    plugins: [terser()]
                }
            ]
        },
        sourcemap: true,
        minify: false, // ✅ Disable Vite’s default minification
        target: "esnext" // ✅ Prevent unnecessary transpilation
    }
});
