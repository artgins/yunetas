# README

## Install

This project uses [`vite`](https://vite.dev/) as build tool.

Install the latest `node`:

    nvm install --lts

When writing this readme the LTS version was:

    node --version
        v22.14.0

    npm install -g vite

Install dependencies:

    npm install

To start Vite dev server:

    vite

To build:

    vite build

To publish a new version of yunetas:

    # change the version in package.json

    npm publish --access public

To test:

    npm test

To test

    npm test

or

    npx vitest

or

    npx vitest --watch  # automatically re-run tests when files change,

or

    npm run test:coverage

## Update

ONLY one time: to update all js packages, install the module::

    npm install -g npm-check-updates

To download new releases::

    ncu -u

And to install the new versions::

    npm install
