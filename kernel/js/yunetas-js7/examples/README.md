# README

# Install

This project uses [`vite`](https://vite.dev/) as build tool.

Install the latest `node`:

    nvm install --lts

When writing this readme the LTS version was:

    node --version
        v22.14.0

This project was created using:

    npm create vite@latest test -- --template vanilla
    cd test
    npm install
    npm run dev

To start Vite dev server:

    vite

To build for production:

    vite build

To preview the production build:

    vite preview

To test

    npm test
or

    npm run test:coverage

## Update

ONLY one time: to update all js packages, install the module::

    npm install -g npm-check-updates

To download new releases::

    ncu -u

And to install the new versions::

    npm install
