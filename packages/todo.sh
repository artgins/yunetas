#----------------------------------------#
#   Copy binaries
#   TODO pending
#    ncurses
#    nginx
#    openresty
#    restart-yuneta
#    ssl3
#   colas2.sh,copy_certs, all about letsencrypt
#   json_diff ?
#----------------------------------------#

cp -a "/yuneta/agent" "$PACKAGE/yuneta/"
cp -a --dereference "/yuneta/bin/nginx/" "$PACKAGE/yuneta/bin/"
cp -a --dereference "/yuneta/bin/ncurses/" "$PACKAGE/yuneta/bin/"
cp -a --dereference "/yuneta/share/" "$PACKAGE/yuneta/share/"
cp "/yuneta/agent/service/yuneta_agent" "$PACKAGE/etc/init.d"

sudo chown yuneta:yuneta "$PACKAGE" -R



cat <<EOF > "./$PACKAGE/etc/profile.d/yuneta.sh"
export PATH=\$PATH:/yuneta/bin
EOF

STRINGX=$(du -s "$PACKAGE/yuneta")
ARRAYX=( "$STRINGX" )
SIZEX=${ARRAYX[0]}

cat <<EOF > "./$PACKAGE/DEBIAN/conffiles"
/etc/init.d/yuneta_agent
/etc/profile.d/yuneta.sh
EOF

cat <<EOF > "./$PACKAGE/DEBIAN/control"
Package: yuneta-agent
Version: $VERSION
Architecture: $ARCHITECTURE
Maintainer: ArtGins.
Section: net
Installed-Size: $SIZEX
Homepage: yuneta.io
Priority: Optional
Depends: debconf, sudo, rsync, tree, vim, curl, adduser, libc6, libssl1.1
Description: Yuneta agent run-time.
 Install this run-time and be a Yuneta's node. Search and Select the Realms to Belong.
EOF

cat <<EOF > "./$PACKAGE/DEBIAN/postinst"
#!/bin/sh
# postinst script for yuneta
#
# see: dh_installdeb(1)

set -e

setup_yuneta_user() {
    if ! getent group yuneta >/dev/null; then
        addgroup --quiet --system yuneta
    fi

    if ! getent passwd yuneta >/dev/null; then
        adduser --quiet --system --ingroup yuneta --shell /bin/bash yuneta
    fi
}

fix_permissions() {
    chown yuneta:yuneta /yuneta -R
    find /yuneta -type d -exec chmod g+s {} \;
    find /yuneta -type d -exec chmod g+w {} \;
    find /yuneta -type f -exec chmod g+w {} \;
}

case "\$1" in
    configure)
        setup_yuneta_user
        fix_permissions
    ;;

    abort-upgrade|abort-remove|abort-deconfigure)
    ;;

    *)
        echo "postinst called with unknown argument '\$1'" >&2
        exit 1
    ;;
esac

if [ "\$1" = "configure" ] || [ "\$1" = "abort-upgrade" ]; then
    if [ -x "/etc/init.d/yuneta_agent" ]; then
        update-rc.d yuneta_agent defaults >/dev/null
    fi
    if [ -x "/etc/init.d/yuneta_agent" ]; then
        invoke-rc.d yuneta_agent start || exit \$?
    fi
fi
# End automatically added section

exit 0
EOF


cat <<EOF >./$PACKAGE/DEBIAN/postrm
#!/bin/sh
# postrm script for yuneta_agent
#
# see: dh_installdeb(1)

set -e

case "\$1" in
    remove|purge)
        rm -f /etc/init.d/yuneta_agent
        update-rc.d -f yuneta_agent remove >/dev/null
    ;;

    upgrade|failed-upgrade)
    ;;

    *)
        echo "postrm called with unknown argument '\$1'" >&2
        exit 1
    ;;
esac

# In case this system is running systemd, we make systemd reload the unit files
# to pick up changes.
if [ -d /run/systemd/system ] ; then
    systemctl --system daemon-reload >/dev/null || true
fi
# End automatically added section

exit 0
EOF


cat <<EOF > "./$PACKAGE/DEBIAN/prerm"
#!/bin/sh
# prerm script for yuneta_agent
#
# see: dh_installdeb(1)

set -e

case "\$1" in
    remove|purge|deconfigure)
        if [ -x /etc/init.d/yuneta_agent ]; then
            if [ -x /usr/sbin/invoke-rc.d ]; then
                invoke-rc.d yuneta_agent stop
            else
                /etc/init.d/yuneta_agent stop
            fi
        fi
    ;;

    upgrade)
    ;;
    failed-upgrade)
    ;;

    *)
        echo "prerm called with unknown argument '\$1'" >&2
        exit 1
    ;;
esac

if [ -x "/etc/init.d/yuneta_agent" ]; then
    invoke-rc.d yuneta_agent stop || exit \$?
fi
# End automatically added section


exit 0
EOF

chmod +x "./$PACKAGE/DEBIAN/postinst"
chmod +x "./$PACKAGE/DEBIAN/postrm"
chmod +x "./$PACKAGE/DEBIAN/prerm"

# chown root:root -R $PACKAGE
rm -f "$BASE_DESTINE/$PACKAGE.deb"

#----------------------------------------#
#   Build the deb
#----------------------------------------#
dpkg -b "$PACKAGE"

cp "$BASE_DESTIN/$PACKAGE.deb" .

#   Useful commands:
#   $ sudo dpkg -i <paquete.deb>            # To install the package WITHOUT installing dependencies
#   $ sudo apt -y install ./<paquete.deb>   # To install the package WITH dependencies
#
