#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="slab"

(test -f $srcdir/configure.in \
  && test -f $srcdir/autogen.sh) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

DIE=0

rm -f .using-mate-libs-package

if ! which mate-autogen ; then
  echo "You need to install the mate-common module and make"
  echo "sure the mate-autogen script is in your \$PATH."
  exit 1
fi

REQUIRED_AUTOMAKE_VERSION=1.9 . mate-autogen
