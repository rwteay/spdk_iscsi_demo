#!/usr/bin/env sh
find patches -type f -name 'spdk-win-0*.patch' -print0 | sort -z | xargs -t -0 -n 1 patch -p1 --forward -i || true
