#!/bin/sh
#
# Take the file game/lockout.cnf and game/sites.cnf and combine them,
# appending the result to game/access.cnf
#
# Usage:
#   make_access_cnf.sh game-directory
# (Commonly called by 'make access', which runs make_access_cnf.sh game)
#
dir=$1

if [ -z "$dir" ]; then
  echo "Usage: make_access_cnf.sh <game-directory>"
  exit 0
fi

if [ ! -d $dir ]; then
  echo "No such directory: $dir"
  exit 0
fi

if [ -r $dir/lockout.cnf ]; then
  echo "Processing lockout.cnf."
  sed -e 's/$/ none/' < $dir/lockout.cnf >> $dir/access.cnf
else
  echo "No lockout.cnf found."
fi


if [ -r $dir/sites.cnf ]; then
  echo "Processing sites.cnf."
  sed -e 's/$/ !create/' < $dir/sites.cnf >> $dir/access.cnf
else
  echo "No sites.cnf found."
fi

echo "Done."

