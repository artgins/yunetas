import { defineConfig } from "vite";
import terser from '@rollup/plugin-terser';

export default defineConfig({
    build: {
        lib: {
            entry: "src/index.js",
            name: "yunetas",
            formats: ["es", "umd"]
        },
        rollupOptions: {
            output: [
                // Non-minified versions
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "yunetas.es.js",
                    sourcemap: false
                },
                {
                    name: "yunetas",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "yunetas.umd.js",
                    sourcemap: false
                },
                // Minified versions
                {
                    format: "es",
                    dir: "dist",
                    entryFileNames: "yunetas.es.min.js",
                    sourcemap: true,
                    plugins: [terser()] // ✅ Properly using terser
                },
                {
                    name: "yunetas",
                    format: "umd",
                    dir: "dist",
                    entryFileNames: "yunetas.umd.min.js",
                    sourcemap: true,
                    plugins: [terser()] // ✅ Properly using terser
                }
            ]
        }
    }
});
