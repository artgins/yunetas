# -*- coding:utf-8 -*-
from mako import runtime, filters, cache
UNDEFINED = runtime.UNDEFINED
STOP_RENDERING = runtime.STOP_RENDERING
__M_dict_builtin = dict
__M_locals_builtin = locals
_magic_number = 10
_modified_time = 1678968636.723325
_enable_loop = True
_template_filename = '/yuneta/development/yuneta/^yuneta/yuno_gui/v4/code/index.mako'
_template_uri = 'index.mako'
_source_encoding = 'utf-8'
_exports = []


def render_body(context,**pageargs):
    __M_caller = context.caller_stack._push_frame()
    try:
        __M_locals = __M_dict_builtin(pageargs=pageargs)
        assets_env = context.get('assets_env', UNDEFINED)
        metadata = context.get('metadata', UNDEFINED)
        title = context.get('title', UNDEFINED)
        __M_writer = context.writer()
        __M_writer('<!doctype html>\n<html class="no-js" lang="">\n    <head>\n        <meta charset="utf-8">\n        <title>')
        __M_writer(str(title))
        __M_writer('</title>\n        <meta name="viewport" content="width=device-width, initial-scale=1">\n\n        <link rel="manifest" href="site.webmanifest">\n        <link rel="apple-touch-icon" href="icon.png">\n        <!-- Place favicon.ico in the root directory -->\n\n')
        for key, value in metadata.items():
            if value:
                __M_writer('        <meta name="')
                __M_writer(str(key))
                __M_writer('" content="')
                __M_writer(str(value))
                __M_writer('">\n')
        __M_writer('\n')
        if 'css' in assets_env:
            for url in assets_env['css']:
                __M_writer('        <link rel="stylesheet" href="')
                __M_writer(str(url))
                __M_writer('">\n')
        __M_writer('\n')
        if 'module_js' in assets_env:
            for url in assets_env['module_js']:
                __M_writer('        <script type="module" src="')
                __M_writer(str(url))
                __M_writer('"></script>\n')
        __M_writer('\n')
        if 'top_js' in assets_env:
            for url in assets_env['top_js']:
                __M_writer('        <script src="')
                __M_writer(str(url))
                __M_writer('"></script>\n')
        __M_writer('    </head>\n    <body>\n        <div id="loading-message"\n            style="border: 1px solid blue; margin: 1em; padding: 2em; background-color: #f8f1fd;">\n            <strong>Loading application. Wait please...</strong>\n        </div>\n\n        <!--  z-index 2 and 3 busy by login form -->\n        <div style="z-index:1;" id="gui_canvas"></div>\n\n')
        if 'bottom_js' in assets_env:
            for url in assets_env['bottom_js']:
                __M_writer('        <script src="')
                __M_writer(str(url))
                __M_writer('"></script>\n')
        __M_writer('\n    </body>\n</html>\n')
        return ''
    finally:
        context.caller_stack._pop_frame()


"""
__M_BEGIN_METADATA
{"filename": "/yuneta/development/yuneta/^yuneta/yuno_gui/v4/code/index.mako", "uri": "index.mako", "source_encoding": "utf-8", "line_map": {"16": 0, "24": 1, "25": 5, "26": 5, "27": 12, "28": 13, "29": 14, "30": 14, "31": 14, "32": 14, "33": 14, "34": 17, "35": 18, "36": 19, "37": 20, "38": 20, "39": 20, "40": 23, "41": 24, "42": 25, "43": 26, "44": 26, "45": 26, "46": 29, "47": 30, "48": 31, "49": 32, "50": 32, "51": 32, "52": 35, "53": 45, "54": 46, "55": 47, "56": 47, "57": 47, "58": 50, "64": 58}}
__M_END_METADATA
"""
