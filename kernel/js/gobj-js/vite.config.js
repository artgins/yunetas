import { defineConfig } from "vite";
import terser from "@rollup/plugin-terser";

export default defineConfig({
    build: {
        lib: {
            entry: "src/index.js",
            name: "gobjJs"
        },
        test: {
            globals: true,  // Use global `describe` and `test` like Jest
            environment: "node",  // Use Node.js or browser as test environment
            coverage: {
                reporter: ["text", "json", "html"],  // Test coverage output
            },
        },
        rollupOptions: {
            output: [
                // Non-minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "gobj-js.es.js",
                    compact: false // ✅ Keep original formatting
                },
                // Non-minified UMD Module
                {
                    name: "gobjJs",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "gobj-js.umd.js",
                    compact: false
                },
                // Non-minified CJS Module
                {
                    name: "gobjJs",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "gobj-js.cjs.js",
                    compact: false
                },
                // Non-minified IIFE Module
                {
                    name: "gobjJs",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "gobj-js.iife.js",
                    compact: false
                },

                // Minified ES Module
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "gobj-js.es.min.js",
                    plugins: [terser()] // ✅ Minified version
                },
                // Minified UMD Module
                {
                    name: "gobjJs",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "gobj-js.umd.min.js",
                    plugins: [terser()]
                },
                // Minified CJS Module
                {
                    name: "gobjJs",
                    format: "cjs",
                    dir: "dist",
                    entryFileNames: "gobj-js.cjs.min.js",
                    plugins: [terser()]
                },
                // Minified IIFE Module
                {
                    name: "gobjJs",
                    format: "iife",
                    dir: "dist",
                    entryFileNames: "gobj-js.iife.min.js",
                    plugins: [terser()]
                }
            ]
        },
        sourcemap: true,
        minify: false, // ✅ Disable Vite’s default minification
        target: "esnext" // ✅ Prevent unnecessary transpilation
    }
});
