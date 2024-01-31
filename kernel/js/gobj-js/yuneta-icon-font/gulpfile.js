/* eslint-env node */
const gulp     = require('gulp')
const iconfont = require('gulp-iconfont')
const fs = require("fs");

let tasks = {

    icons(cb) {
        let fs  = require('fs')
        let css = `@font-face {
    font-family: "yuneta-icon-font";
    src: url("yuneta-icon-font.ttf") format("truetype");
    font-weight: normal;
    font-style: normal;
}
[class^="yuneta-icon-"]:before,
[class*=" yuneta-icon-"]:before {
    font-family: "yuneta-icon-font";
    display: inline-block;
    vertical-align: middle;
    line-height: 1;
    font-weight: normal;
    font-style: normal;
    speak: none;
    text-decoration: inherit;
    text-transform: none;
    text-rendering: optimizeLegibility;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
}
`
        let html = `<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link href="./dist/yuneta-icon-font.css" rel="stylesheet">
    <title>yuneta-icon-font</title>
    <style>
        body { font-family: verdana; font-size: 13px }
        .preview { padding: 8px; margin: 4px; width: 200px; box-shadow: 1px 1px 2px #ccc; float: left }
        .preview:hover { background-color: #f5f5f5 }
        .preview span.icon { font-size: 16px; padding: 8px }
    </style>
</head>
<body>
    <h1 style="font-family: arial; padding-left: 15px;">yuneta-icon-font $count</h1>
`

        let template_json2 = `(function (exports) {
    "use strict";

    var yuneta_icon_font = $values;

    exports.yuneta_icon_font = yuneta_icon_font;
})(this);
`
        let json = []
        let json2 = {}
        gulp.src(['./icons/*.svg'])
            .pipe(iconfont({
                fontName: 'yuneta-icon-font',
                formats: ['ttf', 'eot', 'woff'],
                // fontHeight: 1000,
                descent: 50,
                // normalize: true,
                preserveAspectRatio: true,
                prependUnicode: false,
                fixedWidth: false,
                // centerHorizontally: true,
                centerVertically: false,
                timestamp: Math.round(Date.now()/1000)
            }))
            .on('error', function (err) {
                this.emit('end')
            })
            .on('glyphs', function(glyphs, options) {
                glyphs = glyphs.sort((a, b) => (a.name > b.name) - (a.name < b.name)) // need reorder f series
                glyphs.forEach(function(glyph, i) {
                    let unicode = glyph.unicode[0].charCodeAt(0);
                    html       += `    <div class="preview"><span class="icon yuneta-icon-${glyphs[i].name}"></span><span>yuneta-icon-${glyphs[i].name}</span></div>\n`
                    css        += `.yuneta-icon-${glyphs[i].name}:before { content: "\\${unicode.toString(16)}" }\n`
                    json.push(glyphs[i].name)
                    json2[glyphs[i].name] = unicode
                })

                html += '    <div style="clear: both; height: 10px;"></div>\n</body>\n</html>'
                html  = html.replace('$count', ' - ' + glyphs.length + ' glyphs')
                fs.writeFileSync('./dist/yuneta-icon-font.css', css)
                fs.writeFileSync('./preview.html', html)
                fs.writeFileSync('./glyphs.json', JSON.stringify(json))

                template_json2 = template_json2.replace('$values', JSON.stringify(json2, null, 8))
                fs.writeFileSync('./dist/yuneta-icon-font.js', template_json2)
            })
            .pipe(gulp.dest('./dist/'))
            .on('end', function () {
                let font = fs.readFileSync('./dist/yuneta-icon-font.ttf')
                let file = fs.readFileSync('./dist/yuneta-icon-font.css', 'utf-8')
                file     = file.replace('src: url("yuneta-icon-font.ttf") format("truetype");',
                    `src: url("data:application/x-font-ttf;charset=utf-8;base64,${font.toString('base64')}") format("truetype");`)
                fs.writeFileSync('./dist/yuneta-icon-font.css', file)
                cb()
            })
    },

    watch(cb) {
        gulp.watch(['icons/*.svg'], tasks.icons)
    }
}

exports.default = gulp.series(tasks.icons)
exports.icons   = gulp.series(tasks.icons)
exports.watch   = gulp.series(tasks.watch)
