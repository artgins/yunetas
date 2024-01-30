# -*- encoding: utf-8 -*-
"""
Utility for creating static html code.
"""

import traceback
import os
import argparse
import sys
import stat
import json
import shutil
import datetime
import os.path
from importlib import import_module

import delegator
from webassets import Environment, Bundle
from webassets.exceptions import FilterError, BundleError, BuildError
from mako.lookup import TemplateLookup
from mako.template import Template
from mako.runtime import Context
from mako.exceptions import TopLevelLookupException

try:
   from . import __version__
except:  # pragma: no cover
   __version__ = '0.0.0'

if sys.version_info[:2] < (3, 5):
   raise RuntimeError('Requires Python 3.5 or better')

from configparser import ConfigParser

import logging
logging.basicConfig(level=logging.DEBUG)

WEB_SKELETON_INI = 'web-skeleton3.ini'

comandos = ['init', 'skeleton', 'render', 'rsync', 'version']

try:
    from StringIO import StringIO as NativeIO
except ImportError: # pragma: no cover
    from io import StringIO as NativeIO


def getyesno(message, default='y'):
    """ Utility for ask yes or no
    """
    choices = 'Y/n' if default.lower() in ('y', 'yes') else 'y/N'
    choice = input("%s (%s) " % (message, choices))
    values = ('y', 'yes', '') if default == 'y' else ('y', 'yes')
    return True if choice.strip().lower() in values else False


def getstring(message, default=''):
    """ Utility for ask a string
    """
    value = input("%s " % (message,))
    if not value:
        value = default
    return value

def tostr(s):
    if isinstance(s, str):
        s = s.encode('latin-1')
    return str(s, 'latin-1', 'strict')

def main(argv=sys.argv):
    command = WebSkeleton(argv)
    return command.run()


class WebSkeleton(object):
    description = 'Generator of static html code.'
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "command",
        choices=comandos,
        help="Version: " + __version__ + "\n"
"Available commands (WARNING: to be executed in the web-skeleton3.ini directory):\n"
"init {project} ==> create a .ini file and the directory structure.\n"
"skeleton ==> create a new tag directory, copying a assets directory.\n"
"render ==> generate a new index.html, in tags/{version} directory.\n"
"rsync ==> syncronize the tag version with the remote host.\n"

"\nOnce a skeleton has been created:\n"
"   - Create new js/css/scss files in `code` directory (for example with yuno-skeleton)\n"
"       - Update the `code/config.json` file with the new files in 'tag' directory\n"
"   - Render (web-skeleton3 render)\n"
"\n"
    )
    parser.add_argument(
        "arguments",
        nargs=argparse.REMAINDER,
        help="Arguments to command."
    )
    parser.add_argument(
        '-d',
        '--debug',
        dest='debug',
        action='store_true',
        help="True if render in development mode, False in production mode.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action='store_true',
        help="Increase output verbosity",
    )

    def __init__(self, argv=sys.argv):
        self.args = self.parser.parse_args(argv[1:])
        if self.args.debug:
            self.args.verbose = True

    def run(self):
        cmd = self.args.command
        fn = getattr(self, cmd)
        if cmd not in ['init', 'version']:
            self.load_ini()

        return fn()

    def version(self):
        print("Version %s" % __version__)
        return 0

    def init(self):
        """ create a .ini file and the directory structure
        """
        if not self.args.arguments:
            print('\nERROR: you must supply a project name!\n')
            return 2
        project = self.args.arguments[0].lower()
        output_dir = os.path.abspath(os.path.normpath(project))
        if os.path.exists(output_dir):
            print('\nERROR: directory "%s" already exists!\n' % (output_dir))
            return 2

        #
        #   Copy `setup` directory
        #
        src_path = os.path.join(self.module_dir(), "setup")
        dst_path = output_dir
        shutil.copytree(
            src_path,
            dst_path,
            symlinks=True,
        )

        #
        #   Copy `assets` directory
        #
        src_path = os.path.join(self.module_dir(), "assets")
        dst_path = os.path.join(output_dir, 'assets')
        shutil.copytree(
            src_path,
            dst_path,
            symlinks=True
        )

        #
        #   Copy `code` directory
        #
        src_path = os.path.join(self.module_dir(), "code")
        dst_path = os.path.join(output_dir, 'code')
        shutil.copytree(
            src_path,
            dst_path,
            ignore=shutil.ignore_patterns('*.pyc', '*~'),
            symlinks=True,
        )

        #
        #   Create `tags` directory
        #
        os.mkdir(os.path.join(output_dir, 'tags'))

        #
        #   Output some info
        #
        print("\nOK: created new web-skeleton project in '%s'.\n" % output_dir)
        print("Next steps:")
        print("   - Go to '%s' directory (`cd %s`)" % (project, project))
        print("   - Check your settings in web-skeleton3.ini file")
        print("   - Build a new h5bp structure (web-skeleton3 skeleton)")
        print("")

        return 0

    def load_ini(self):
        if not os.path.exists(WEB_SKELETON_INI):
            print('\nERROR: file "%s" NOT found.\n' % (WEB_SKELETON_INI))
            exit(2)
        # 'here' is the directory of the .ini file
        here = os.path.dirname(os.path.abspath(WEB_SKELETON_INI))
        self.config = ConfigParser({'here': here})
        self.config.read(WEB_SKELETON_INI)
        self.config.here = here
        self.config.current_tag = self.config.get('DEFAULT', 'current_tag')

        json_file = os.path.join(self.code_dir(), 'config.json')
        with open(json_file) as data_file:
            self.config.data = json.load(data_file)
        if "title" not in self.config.data:
            self.config.data["title"] = ""
        if "metadata" not in self.config.data:
            self.config.data["metadata"] = {}

    def module_dir(self):
        mod = sys.modules[self.__class__.__module__]
        return os.path.dirname(mod.__file__)

    def code_dir(self):
        return os.path.join(self.config.here, "code")

    def current_tag_dir(self):
        return os.path.join(
            self.config.here,
            'tags',
            self.config.current_tag,
        )

    def skeleton(self):
        """ create a new tag directory, copying an assets directory
        """
        dst_path = self.current_tag_dir()

        if not os.path.exists(dst_path):
            pass # print('<---- * Creating "%s" directory.' % (dst_path))
        else:
            resp = getyesno(
                'WARNING: You are re-creating the skeleton "%s".\n'
                'You will loose your data! Are you sure?' % dst_path,
                default='n',
            )
            if not resp:
                print('\nOperation aborted.\n')
                return 2
            shutil.rmtree(dst_path)

        #
        #   Copy `h5bp` directory (default, you can change
        #   in assets of web-skeleton3.ini file)
        #
        assets = self.config.get(self.config.current_tag, 'assets')
        src_path = os.path.join(
            self.config.here,
            assets,
        )
        try:
            shutil.copytree(
                src_path,
                dst_path,
                symlinks=True,
            )
        except OSError:
            pass

        self.link("code/app")

        print("\nOK: Created '%s' tag.\n" % (dst_path))

        print("Next steps:")
        print("   - Create new js/css/scss files in `code` directory (for example with yuno-skeleton)")
        print("       - Use `web-skeleton3 link` command to link new files from 'code/?' to 'tag' directory")
        print("       - Update the `code/config.json` file with the new links in 'tag' directory")
        print("   - Render (web-skeleton render)")
        print("")

        return 0

    def link(self, path):
        """ creat a new link of js/css/scss file
        """
        if not os.path.exists(self.current_tag_dir()):
            print('\nWARNING: firstly you must create a tag skeleton.\n')
            exit(2)

        #if not self.args.arguments:
            #print('WARNING: You must supply a file name!')
            #exit(2)
        #path = self.args.arguments[0]
        if not os.path.exists(path):
            print("\nWARNING: '%s' not found.\n" % (path,))
            exit(2)

        source = os.path.join(
            self.config.here,
            path
        )
        subpath = path[path.index('/')+1:]

        target = os.path.join(
            self.current_tag_dir(),
            "./static",
            subpath
        )
        dir_name = os.path.dirname(target)
        if not os.path.exists(dir_name):
            os.mkdir(dir_name)

        try:
            os.symlink(source, target)
        except FileExistsError:
            pass

        #print("\nOK: Created '%s' link.\n" % (target))

        #print("Next steps:")
        #print("   - Update the `code/config.json` file with the new link:")
        #print("         %s" % (subpath,))
        #print("   - Render (web-skeleton render)")
        #print("")

        return 0

    def render(self, **kw):
        """ generate a new index.html, in tags/{version} directory
        WebSkeleton will render using next call code:
        """
        project = os.path.basename(self.config.here).lower()
        if self.args.verbose:
            print('Rendering ' + project + '...')
        output_path = self.current_tag_dir()
        if not os.path.exists(output_path):
            print('\nWARNING: firstly you must create a tag skeleton.\n')
            exit(2)

        os.chdir(self.config.here)

        output_path = self.current_tag_dir()
        code_path = self.code_dir()

        mako_lookup = self.get_mako_lookup(
            code_path,
            output_path
        )

        assets_css_env = self.get_assets_css_env(
            code_path,
            output_path
        )
        assets_top_js_env = self.get_assets_top_js_env(
            code_path,
            output_path
        )
        assets_module_js_env = self.get_assets_module_js_env(
            code_path,
            output_path
        )
        assets_bottom_js_env = self.get_assets_bottom_js_env(
            code_path,
            output_path
        )
        kw.update(**self.config.data)
        assets_env = {}

        some_error = False
        try:
            assets_env["css"] = assets_css_env["css"].urls()
        except Exception as error:
            some_error = True
            print(error)
            pass

        try:
            assets_env["top_js"] = assets_top_js_env["top_js"].urls()
        except Exception as error:
            some_error = True
            print(error)
            pass

        try:
            assets_env["module_js"] = assets_module_js_env["module_js"].urls()
        except Exception as error:
            some_error = True
            print(error)
            pass

        try:
            assets_env["bottom_js"] = assets_bottom_js_env["bottom_js"].urls()
        except Exception as error:
            some_error = True
            print(error)
            pass

        if some_error:
            return -1

        kw.update(assets_env=assets_env)

        buf = NativeIO()

        ctx = Context(buf, **kw)
        template = mako_lookup.get_template("index.mako")

        if template:
            template.render_context(ctx)

        html = buf.getvalue()
        html = tostr(html)

        buf.close()

        if self.args.verbose:
            print(html)

        index_html = os.path.join(output_path, 'index.html')
        fd = open(index_html, 'w')
        fd.write(html)
        fd.close()
        print('\nOK: Created "%s" file.\n' % (index_html,))
        return 0

    def get_mako_lookup(self, code_path, output_path):
        cache_dir = os.path.join(
            output_path,
            '.cache',
        )
        if not os.path.exists(cache_dir):
            os.mkdir(cache_dir)

        lookup = TemplateLookup(
            directories=[code_path],
            module_directory=cache_dir,
        )
        return lookup

    def get_assets_css_env(self, code_path, output_path):
        """ The directory structure of assets is:
            output_path
                static
                    css
                    js
        """
        output_path = os.path.join(output_path, "./static")
        assets_env = Environment(output_path, "./static", debug=self.args.debug)

        assets_env.config['compass_config'] = {
            'additional_import_paths': [
                os.path.join(code_path, 'scss-mixins')
            ],
            #'sass_options': "cache: False", ??? i can't get it.
            'http_path': "./static",
        }

        css_list = []
        scss_list = []

        if "css_or_scss" in self.config.data:
            for filename in self.config.data["css_or_scss"]:
                if self.args.verbose:
                    print('Adding filename: %s' % (filename))
                ext = os.path.splitext(filename)[1]
                if ext == '.scss':
                    scss_list.append(filename)
                elif ext == '.css':
                    css_list.append(filename)
                else:
                    raise Exception('Bad extension: is %s instead of css/scss' % ext)

        if len(css_list) or len(scss_list):
            css_bundle = Bundle(*css_list)

            scss_bundle = []
            for scss_file in scss_list:
                if self.args.verbose:
                    print('Processing filename: %s' % (scss_file))
                try:
                    xxx = Bundle(
                        scss_file,
                        filters='compass',
                        output=scss_file + '.css',
                    )
                    scss_bundle.append(xxx)
                except Exception as error:
                    print("ERROR processing filename '%s': %s" % (scss_file, error))

            css = Bundle(
                css_bundle,
                *scss_bundle,
                filters='cssutils,cssrewrite',
                output='packed.css'
            )
            assets_env.register('css', css)

        return assets_env

    def get_assets_top_js_env(self, code_path, output_path):
        """ The directory structure of assets is:
            output_path
                static
                    css
                    js
        """
        output_path = os.path.join(output_path, "./static")
        assets_env = Environment(output_path, "./static", debug=self.args.debug)
        assets_env.config['compass_config'] = {
            'additional_import_paths': [
                os.path.join(code_path, 'scss-mixins')
            ],
            #'sass_options': "cache: False", ??? i can't get it.
            'http_path': "./static",
        }

        top_js_list = []
        if "top_js" in self.config.data:
            top_js_list = self.config.data["top_js"]
            # if top_js_list and len(top_js_list):
        top_js = Bundle(
            *top_js_list,
            filters='rjsmin',
            output='top.js'
        )
        assets_env.register('top_js', top_js)

        return assets_env

    def get_assets_module_js_env(self, code_path, output_path):
        """ The directory structure of assets is:
            output_path
                static
                    css
                    js
        """
        output_path = os.path.join(output_path, "./static")
        assets_env = Environment(output_path, "./static", debug=self.args.debug)
        assets_env.config['compass_config'] = {
            'additional_import_paths': [
                os.path.join(code_path, 'scss-mixins')
            ],
            #'sass_options': "cache: False", ??? i can't get it.
            'http_path': "./static",
        }

        module_js_list = []
        if "module_js" in self.config.data:
            module_js_list = self.config.data["module_js"]
            # if module_js_list and len(module_js_list):
            #     module_js = Bundle(
            #         *module_js_list,
            #         filters='rjsmin',
            #         output='module.js'
            #     )
            #     assets_env.register('module_js', module_js)

        module_js = Bundle(
            *module_js_list,
            filters='rjsmin',
            output='module.js'
        )
        assets_env.register('module_js', module_js)

        return assets_env

    def get_assets_bottom_js_env(self, code_path, output_path):
        """ The directory structure of assets is:
            output_path
                static
                    css
                    js
        """
        output_path = os.path.join(output_path, "./static")
        assets_env = Environment(output_path, "./static", debug=self.args.debug)
        assets_env.config['compass_config'] = {
            'additional_import_paths': [
                os.path.join(code_path, 'scss-mixins')
            ],
            #'sass_options': "cache: False", ??? i can't get it.
            'http_path': "./static",
        }

        bottom_js_list = []
        if "bottom_js" in self.config.data:
            bottom_js_list = self.config.data["bottom_js"]
            # if bottom_js_list and len(bottom_js_list):
        bottom_js = Bundle(
            *bottom_js_list,
            filters='rjsmin',
            output='bottom.js'
        )
        assets_env.register('bottom_js', bottom_js)

        return assets_env

    def rsync(self):
        """ syncronize the tag version with the remote host
        """
        src_path = os.path.join(self.current_tag_dir(), '')
        remote = self.config.get(self.config.current_tag, 'remote-server')
        if not remote:
            print('\nWARNING: Please specify a remote server path.\n')
            exit(2)
        print('rsyncing...')

        command = r'rsync -avzL --delete ' \
            r'--exclude \.webassets-cache --exclude \.sass-cache --exclude \.cache %s %s' % (
                src_path,
                remote,
            )
        if self.args.verbose:
            print(command)
        response = delegator.run(command)
        print(response.out)

        return 0


if __name__ == '__main__':
    main()
