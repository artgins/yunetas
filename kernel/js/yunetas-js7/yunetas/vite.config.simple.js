import { defineConfig } from "vite";

export default defineConfig(({ mode = "development" }) => {
    const isProduction = mode === "production";

    return {
        build: {
            lib: {
                entry: "src/index.js",
                name: "Yunetas",
                fileName: (format) => isProduction
                    ? `yunetas.${format}.min.js`
                    : `yunetas.${format}.js`,
                formats: ["es", "umd"]
            },
            minify: isProduction ? "terser" : false, // Force minification only in production
            sourcemap: true
        }
    };
});
