#!/bin/sh
ver="${1-1.7}"
name="miniupnpc-${ver}"
curl -o "${name}.tar.gz" "http://miniupnp.free.fr/files/${name}.tar.gz"
tar -xvzf "${name}.tar.gz"
cd "${name}" || exit 1
make || exit 1
make install
# eof
