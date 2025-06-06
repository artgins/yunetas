ORIGIN="./_build/html/"
DESTINE="yuneta@yuneta.io:/yuneta/gui/doc.yuneta.io/"
rsync -auvzL -e ssh \
    --exclude=.sass-cache --exclude=.webassets-cache --exclude=.svn  --exclude=.hg  --exclude=.git --exclude=*.db \
    --exclude=dist --exclude=build --exclude=*.pyc --exclude=.kdev4 --exclude=*.kdev4 --exclude=.cache \
    --exclude=.libs --exclude=*.o --exclude=.deps \
    --exclude=.hgtags --exclude=.la --exclude=.lo \
    $ORIGIN $DESTINE

