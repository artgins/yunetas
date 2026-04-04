#!/bin/sh
vite build
watchfs -f watch.json
