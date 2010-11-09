#!/bin/sh

# update via SVN
svn up

# find out the current revision
SVNVERSION="$(export LANG=C && export LC_ALL=C && echo $(svn info | awk '/^Revision:/ { print $2 }'))"

# use checkinstall to create the DEB package
sudo checkinstall -D \
                  --pkgname "mjpg-streamer" \
                  --pkgversion "r$SVNVERSION" \
                  --pkgrelease "1" \
                  --maintainer "tom_stoeveken@users.sourceforge.net" \
                  --requires "libjpeg62" \
                  --nodoc \
                    make DESTDIR=/usr install
