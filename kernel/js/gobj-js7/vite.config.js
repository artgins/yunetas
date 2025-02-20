import { defineConfig } from "vite";

export default defineConfig({
    build: {
        lib: {
            entry: "src/index.js", // Entry file
            name: "YunetaS", // UMD Global Variable
            fileName: (format) => `yunetas.${format}.js`,
            formats: ["es", "umd"] // Generate ES module and UMD
        },
        rollupOptions: {
            output: {
                globals: {
                    // Define global dependencies if needed
                }
            }
        }
    }
});
