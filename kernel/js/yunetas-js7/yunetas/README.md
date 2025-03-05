# README

This project uses [`vite`](https://vite.dev/) as build tool.

Install the latest `node`:

    nvm install --lts

When writing this readme the LTS version was:

    node --version
        v22.14.0

Install dependencies:

    npm install

To start Vite dev server:

    vite

To build:

    vite build

To publish a new version of yunetas:
    
    # change the version in package.json

    npm publish --access public

To test

    npm test
or

    npm run test:coverage
