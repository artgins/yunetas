inkscape yuneta-y.svg --export-filename=logo.png -w 100 -h 100
inkscape yuneta-y.svg --export-filename=icon.png -w 100 -h 100
inkscape yuneta-y.svg --export-filename=favicon.png -w 64 -h 64; rm favicon.ico; mv favicon.png favicon.ico;
inkscape yuneta-y-wide.svg --export-filename=tile-wide.png  -h 100
inkscape yuneta-y-wide.svg --export-filename=logo-wide.png  -h 100
inkscape yuneta-y.svg --export-filename=tile.png -w 100 -h 100
cp *.png *.svg *.ico ../../public/
