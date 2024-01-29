Modifies to generate not minified pikaso
========================================


Modify next files
-----------------

At ``package.json``::

    "konva": "^8.3.8",    // update to last versiom

    "unpkg": "umd/pikaso.js",


At ``rollup.config.js``::
    {
        plugins: [
        ignore(['canvas']),
        nodeResolve(),
        commonjs({
            exclude: '/node_modules/'
        }),
        typescript(),
        inject({
            global: path.resolve('./build/global')
        }),
        // terser() GMS
        ],
        input: 'src/index.ts',
        output: {
        name: 'Pikaso',
        file: pkg.unpkg,
        format: 'iife',
        freeze: false,
        sourcemap: false // GMS
        }
    }

Execute::

    npm run build

Si falla::

    npm install rollup
