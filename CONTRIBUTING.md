# Contributing to Yuneta

Thank you for wanting to improve Yuneta. This file says how a change gets in.
The same rules hold for the JavaScript and Python projects of Yuneta
(`gobj-js`, `gobj-ui.js`, `yunos-js`, `tui_yunetas.py`), which point here.

## Before you start

- For anything beyond a small fix, open an issue first and say what you want
  to change and why: the shape of a change is cheaper to agree on before it is
  written. Yuneta is a consolidated project: "obvious" changes have broken
  invariants before.
- Read `CLAUDE.md`. Whoever writes the code, it is the guide of this
  repository: the `gbmem_*` allocator (never `malloc`/`free`), braces on every
  block, no silent errors, the gclass skeletons, the subscription models, and
  the build and test conventions.
- Everything committed is in English: code, comments, docs, commit messages.
  The conversation can be in any language.

## Build and test

```sh
source yunetas-env.sh
yunetas init        # the first time, or after .config changes
yunetas build
yunetas test        # builds the tests and runs ctest
```

The external libraries are built once before that (`kernel/c/linux-ext-libs`:
`./extrae.sh && ./configure-libs.sh`); `docs/doc.yuneta.io/installation.md`
has the whole setup. A change comes with the test that shows it: a fix with
the case that failed, a feature with the cases it promises. A change of
behavior is said in `CHANGELOG.md` (under `Unreleased`, with the why) and in
the docs of `docs/doc.yuneta.io/`; a new public function has its entry, with
an example (`scripts/verify_api_coverage.py` checks it).

## Code of others

Do not paste code you did not write unless its license allows it in an MIT
project and you say where it comes from. Then the code keeps its notice, and
its license goes in `THIRD-PARTY-NOTICES` in the same pull request; the
packages install that file. Code under the GPL, AGPL or LGPL, under CC BY-SA
(answers of StackOverflow and other Q&A sites), or with no license at all
(most gists) cannot come in. A library added to `kernel/c/linux-ext-libs`
updates `THIRD-PARTY-NOTICES` too.

Code written with an AI assistant is welcome, under the same rule: you have
read it, you answer for it, and you say so in the commit (a `Co-Authored-By:`
line).

## Contributor License Agreement

Before ArtGins can merge your first pull request, you must agree to the
[Contributor License Agreement](CLA.md) of the Yuneta projects. You keep the
copyright of your contribution; the CLA gives ArtGins the right to distribute
it under the license of the project and under other licenses, which is what
lets Yuneta be offered under more than one license. To agree, write this in
the description of your pull request, with your name and email:

    I have read the CLA of the Yuneta projects (CLA.md, version 1.0) and I
    agree to it for this contribution and my future ones.
    Name: <your name>   Email: <your email>

If you contribute as part of your job, your employer may have to sign a
corporate agreement first: write to support@artgins.com.

## License

Yuneta is under the MIT License of `LICENSE.txt`. By contributing, you agree
that your contribution is distributed under it, and under the terms of the
CLA.
