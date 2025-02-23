import { defineConfig } from "vite";
import terser from "@rollup/plugin-terser";

export default defineConfig({
    build: {
        lib: {
            entry: "src/index.js",
            name: "yunetas",
            formats: ["es", "umd", "cjs"]
        },
        rollupOptions: {
            output: [
                {
                    format: "es",
                    file: "dist/yunetas.es.js",
                    sourcemap: true,
                    compact: false
                },
                {
                    format: "umd",
                    name: "yunetas",
                    file: "dist/yunetas.umd.js",
                    sourcemap: true,
                    compact: false
                },
                {
                    format: "cjs",
                    file: "dist/yunetas.cjs.js",
                    sourcemap: true,
                    compact: false
                },
                {
                    format: "es",
                    file: "dist/yunetas.es.min.js",
                    plugins: [terser()]
                },
                {
                    format: "umd",
                    name: "yunetas",
                    file: "dist/yunetas.umd.min.js",
                    plugins: [terser()]
                },
                {
                    format: "cjs",
                    file: "dist/yunetas.cjs.min.js",
                    plugins: [terser()]
                }
            ]
        },
        target: "esnext"
    }
});
