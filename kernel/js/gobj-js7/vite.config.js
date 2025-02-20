import { defineConfig } from "vite";

export default defineConfig(({mode}) => {
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
            minify: isProduction ? "esbuild" : false,
            sourcemap: true
        }
    };
});
