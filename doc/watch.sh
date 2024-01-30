#!/bin/sh
rm -rf _build; make html
watchfs -f watch.json
