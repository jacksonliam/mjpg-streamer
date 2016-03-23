#!/bin/sh

# update via git
git pull

# find out the current revision
GITVERSION="$(export LANG=C && export LC_ALL=C && echo $(git show -s --format=%at-%H))"

# run cmake before our checkinstall run
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .

# use checkinstall to create the DEB package
sudo checkinstall -D \
                  --pkgname "mjpg-streamer" \
                  --pkgversion "$GITVERSION" \
                  --pkgrelease "1" \
                  --maintainer "tom_stoeveken@users.sourceforge.net" \
                  --requires "libjpeg62" \
                  --nodoc \
                    make DESTDIR=/usr install
