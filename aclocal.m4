# ===========================================================================
#       http://www.gnu.org/software/autoconf-archive/ax_lib_mysql.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_MYSQL([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   This macro provides tests of availability of MySQL client library of
#   particular version or newer.
#
#   AX_LIB_MYSQL macro takes only one argument which is optional. If there
#   is no required version passed, then macro does not run version test.
#
#   The --with-mysql option takes one of three possible values:
#
#   no - do not check for MySQL client library
#
#   yes - do check for MySQL library in standard locations (mysql_config
#   should be in the PATH)
#
#   path - complete path to mysql_config utility, use this option if
#   mysql_config can't be found in the PATH
#
#   This macro calls:
#
#     AC_SUBST(MYSQL_CFLAGS)
#     AC_SUBST(MYSQL_LDFLAGS)
#     AC_SUBST(MYSQL_VERSION)
#
#   And sets:
#
#     HAVE_MYSQL
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 12

AC_DEFUN([AX_LIB_MYSQL],
[
    AC_ARG_WITH([mysql],
        AS_HELP_STRING([--with-mysql=@<:@ARG@:>@],
            [use MySQL client library @<:@default=yes@:>@, optionally specify path to mysql_config]
        ),
        [
        if test "$withval" = "no"; then
            want_mysql="no"
        elif test "$withval" = "yes"; then
            want_mysql="yes"
        else
            want_mysql="yes"
            MYSQL_CONFIG="$withval"
        fi
        ],
        [want_mysql="yes"]
    )
    AC_ARG_VAR([MYSQL_CONFIG], [Full path to mysql_config program])

    MYSQL_CFLAGS=""
    MYSQL_LDFLAGS=""
    MYSQL_VERSION=""

    dnl
    dnl Check MySQL libraries
    dnl

    if test "$want_mysql" = "yes"; then

        if test -z "$MYSQL_CONFIG" ; then
            AC_PATH_PROGS([MYSQL_CONFIG], [mysql_config mysql_config5], [no])
        fi

        if test "$MYSQL_CONFIG" != "no"; then
            MYSQL_CFLAGS="`$MYSQL_CONFIG --cflags`"
            MYSQL_LDFLAGS="`$MYSQL_CONFIG --libs`"

            MYSQL_VERSION=`$MYSQL_CONFIG --version`

            found_mysql="yes"
        else
            found_mysql="no"
        fi
    fi

    dnl
    dnl Check if required version of MySQL is available
    dnl


    mysql_version_req=ifelse([$1], [], [], [$1])

    if test "$found_mysql" = "yes" -a -n "$mysql_version_req"; then

        AC_MSG_CHECKING([if MySQL version is >= $mysql_version_req])

        dnl Decompose required version string of MySQL
        dnl and calculate its number representation
        mysql_version_req_major=`expr $mysql_version_req : '\([[0-9]]*\)'`
        mysql_version_req_minor=`expr $mysql_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        mysql_version_req_micro=`expr $mysql_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$mysql_version_req_micro" = "x"; then
            mysql_version_req_micro="0"
        fi

        mysql_version_req_number=`expr $mysql_version_req_major \* 1000000 \
                                   \+ $mysql_version_req_minor \* 1000 \
                                   \+ $mysql_version_req_micro`

        dnl Decompose version string of installed MySQL
        dnl and calculate its number representation
        mysql_version_major=`expr $MYSQL_VERSION : '\([[0-9]]*\)'`
        mysql_version_minor=`expr $MYSQL_VERSION : '[[0-9]]*\.\([[0-9]]*\)'`
        mysql_version_micro=`expr $MYSQL_VERSION : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$mysql_version_micro" = "x"; then
            mysql_version_micro="0"
        fi

        mysql_version_number=`expr $mysql_version_major \* 1000000 \
                                   \+ $mysql_version_minor \* 1000 \
                                   \+ $mysql_version_micro`

        mysql_version_check=`expr $mysql_version_number \>\= $mysql_version_req_number`
        if test "$mysql_version_check" = "1"; then
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
        fi
    fi

    if test "$found_mysql" = "yes" ; then
        AC_DEFINE([HAVE_MYSQL], [1],
                  [Define to 1 if MySQL libraries are available])
    fi

    AC_SUBST([MYSQL_VERSION])
    AC_SUBST([MYSQL_CFLAGS])
    AC_SUBST([MYSQL_LDFLAGS])
])
# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_lib_sqlite3.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_SQLITE3([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the SQLite 3 library of a particular version (or newer)
#
#   This macro takes only one optional argument, required version of SQLite
#   3 library. If required version is not passed, 3.0.0 is used in the test
#   of existance of SQLite 3.
#
#   If no intallation prefix to the installed SQLite library is given the
#   macro searches under /usr, /usr/local, and /opt.
#
#   This macro calls:
#
#     AC_SUBST(SQLITE3_CFLAGS)
#     AC_SUBST(SQLITE3_LDFLAGS)
#     AC_SUBST(SQLITE3_VERSION)
#
#   And sets:
#
#     HAVE_SQLITE3
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 14

AC_DEFUN([AX_LIB_SQLITE3],
[
    AC_ARG_WITH([sqlite3],
        AS_HELP_STRING(
            [--with-sqlite3=@<:@ARG@:>@],
            [use SQLite 3 library @<:@default=yes@:>@, optionally specify the prefix for sqlite3 library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_SQLITE3="no"
        elif test "$withval" = "yes"; then
            WANT_SQLITE3="yes"
            ac_sqlite3_path=""
        else
            WANT_SQLITE3="yes"
            ac_sqlite3_path="$withval"
        fi
        ],
        [WANT_SQLITE3="yes"]
    )

    SQLITE3_CFLAGS=""
    SQLITE3_LDFLAGS=""
    SQLITE3_VERSION=""

    if test "x$WANT_SQLITE3" = "xyes"; then

        ac_sqlite3_header="sqlite3.h"

        sqlite3_version_req=ifelse([$1], [], [3.0.0], [$1])
        sqlite3_version_req_shorten=`expr $sqlite3_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        sqlite3_version_req_major=`expr $sqlite3_version_req : '\([[0-9]]*\)'`
        sqlite3_version_req_minor=`expr $sqlite3_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        sqlite3_version_req_micro=`expr $sqlite3_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$sqlite3_version_req_micro" = "x" ; then
            sqlite3_version_req_micro="0"
        fi

        sqlite3_version_req_number=`expr $sqlite3_version_req_major \* 1000000 \
                                   \+ $sqlite3_version_req_minor \* 1000 \
                                   \+ $sqlite3_version_req_micro`

        AC_MSG_CHECKING([for SQLite3 library >= $sqlite3_version_req])

        if test "$ac_sqlite3_path" != ""; then
            ac_sqlite3_ldflags="-L$ac_sqlite3_path/lib"
            ac_sqlite3_cppflags="-I$ac_sqlite3_path/include"
        else
            for ac_sqlite3_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_sqlite3_path_tmp/include/$ac_sqlite3_header" \
                    && test -r "$ac_sqlite3_path_tmp/include/$ac_sqlite3_header"; then
                    ac_sqlite3_path=$ac_sqlite3_path_tmp
                    ac_sqlite3_cppflags="-I$ac_sqlite3_path_tmp/include"
                    ac_sqlite3_ldflags="-L$ac_sqlite3_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_sqlite3_ldflags="$ac_sqlite3_ldflags -lsqlite3"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_sqlite3_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <sqlite3.h>]],
                [[
#if (SQLITE_VERSION_NUMBER >= $sqlite3_version_req_number)
/* Everything is okay */
#else
#  error SQLite version is too old
#endif
                ]]
            )
            ],
            [
            AC_MSG_RESULT([yes])
            success="yes"
            ],
            [
            AC_MSG_RESULT([not found])
            success="no"
            ]
        )
        AC_LANG_POP(C)

        CPPFLAGS="$saved_CPPFLAGS"

        if test "$success" = "yes"; then

            SQLITE3_CFLAGS="$ac_sqlite3_cppflags"
            SQLITE3_LDFLAGS="$ac_sqlite3_ldflags"

            ac_sqlite3_header_path="$ac_sqlite3_path/include/$ac_sqlite3_header"

            dnl Retrieve SQLite release version
            if test "x$ac_sqlite3_header_path" != "x"; then
                ac_sqlite3_version=`cat $ac_sqlite3_header_path \
                    | grep '#define.*SQLITE_VERSION.*\"' | sed -e 's/.* "//' \
                        | sed -e 's/"//'`
                if test $ac_sqlite3_version != ""; then
                    SQLITE3_VERSION=$ac_sqlite3_version
                else
                    AC_MSG_WARN([Cannot find SQLITE_VERSION macro in sqlite3.h header to retrieve SQLite version!])
                fi
            fi

            AC_SUBST(SQLITE3_CFLAGS)
            AC_SUBST(SQLITE3_LDFLAGS)
            AC_SUBST(SQLITE3_VERSION)
            AC_DEFINE([HAVE_SQLITE3], [], [Have the SQLITE3 library])
        fi
    fi
])

# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_lib_postgresql.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_POSTGRESQL([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   This macro provides tests of availability of PostgreSQL 'libpq' library
#   of particular version or newer.
#
#   AX_LIB_POSTGRESQL macro takes only one argument which is optional. If
#   there is no required version passed, then macro does not run version
#   test.
#
#   The --with-postgresql option takes one of three possible values:
#
#   no - do not check for PostgreSQL client library
#
#   yes - do check for PostgreSQL library in standard locations (pg_config
#   should be in the PATH)
#
#   path - complete path to pg_config utility, use this option if pg_config
#   can't be found in the PATH
#
#   This macro calls:
#
#     AC_SUBST(POSTGRESQL_CPPFLAGS)
#     AC_SUBST(POSTGRESQL_LDFLAGS)
#     AC_SUBST(POSTGRESQL_LIBS)
#     AC_SUBST(POSTGRESQL_VERSION)
#
#   And sets:
#
#     HAVE_POSTGRESQL
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#   Copyright (c) 2014 Sree Harsha Totakura <sreeharsha@totakura.in>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 12

AC_DEFUN([AX_LIB_POSTGRESQL],
[
    AC_ARG_WITH([postgresql],
        AS_HELP_STRING([--with-postgresql=@<:@ARG@:>@],
            [use PostgreSQL library @<:@default=yes@:>@, optionally specify path to pg_config]
        ),
        [
        if test "$withval" = "no"; then
            want_postgresql="no"
        elif test "$withval" = "yes"; then
            want_postgresql="yes"
        else
            want_postgresql="yes"
            PG_CONFIG="$withval"
        fi
        ],
        [want_postgresql="yes"]
    )

    POSTGRESQL_CPPFLAGS=""
    POSTGRESQL_LDFLAGS=""
    POSTGRESQL_LIBS=""
    POSTGRESQL_VERSION=""

    dnl
    dnl Check PostgreSQL libraries (libpq)
    dnl

    if test "$want_postgresql" = "yes"; then

        if test -z "$PG_CONFIG" -o test; then
            AC_PATH_PROG([PG_CONFIG], [pg_config], [])
        fi

        if test ! -x "$PG_CONFIG"; then
            AC_MSG_WARN([$PG_CONFIG does not exist or it is not an executable file])
            PG_CONFIG="no"
            found_postgresql="no"
        fi

        if test "$PG_CONFIG" != "no"; then
            AC_MSG_CHECKING([for PostgreSQL libraries])

            POSTGRESQL_CPPFLAGS="-I`$PG_CONFIG --includedir`"
            POSTGRESQL_LDFLAGS="-L`$PG_CONFIG --libdir`"
            POSTGRESQL_LIBS="-lpq"
            POSTGRESQL_VERSION=`$PG_CONFIG --version | sed -e 's#PostgreSQL ##'`

            AC_DEFINE([HAVE_POSTGRESQL], [1],
                [Define to 1 if PostgreSQL libraries are available])

            found_postgresql="yes"
            AC_MSG_RESULT([yes])
        else
            found_postgresql="no"
            AC_MSG_RESULT([no])
        fi
    fi

    dnl
    dnl Check if required version of PostgreSQL is available
    dnl


    postgresql_version_req=ifelse([$1], [], [], [$1])

    if test "$found_postgresql" = "yes" -a -n "$postgresql_version_req"; then

        AC_MSG_CHECKING([if PostgreSQL version is >= $postgresql_version_req])

        dnl Decompose required version string of PostgreSQL
        dnl and calculate its number representation
        postgresql_version_req_major=`expr $postgresql_version_req : '\([[0-9]]*\)'`
        postgresql_version_req_minor=`expr $postgresql_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        postgresql_version_req_micro=`expr $postgresql_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$postgresql_version_req_micro" = "x"; then
            postgresql_version_req_micro="0"
        fi

        postgresql_version_req_number=`expr $postgresql_version_req_major \* 1000000 \
                                   \+ $postgresql_version_req_minor \* 1000 \
                                   \+ $postgresql_version_req_micro`

        dnl Decompose version string of installed PostgreSQL
        dnl and calculate its number representation
        postgresql_version_major=`expr $POSTGRESQL_VERSION : '\([[0-9]]*\)'`
        postgresql_version_minor=`expr $POSTGRESQL_VERSION : '[[0-9]]*\.\([[0-9]]*\)'`
        postgresql_version_micro=`expr $POSTGRESQL_VERSION : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$postgresql_version_micro" = "x"; then
            postgresql_version_micro="0"
        fi

        postgresql_version_number=`expr $postgresql_version_major \* 1000000 \
                                   \+ $postgresql_version_minor \* 1000 \
                                   \+ $postgresql_version_micro`

        postgresql_version_check=`expr $postgresql_version_number \>\= $postgresql_version_req_number`
        if test "$postgresql_version_check" = "1"; then
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
            AC_DEFINE([HAVE_POSTGRESQL], [0],
                [A required version of PostgreSQL is not found])
            POSTGRESQL_CPPFLAGS=""
            POSTGRESQL_LDFLAGS=""
            POSTGRESQL_LIBS=""
        fi
    fi

    AC_SUBST([POSTGRESQL_VERSION])
    AC_SUBST([POSTGRESQL_CPPFLAGS])
    AC_SUBST([POSTGRESQL_LDFLAGS])
    AC_SUBST([POSTGRESQL_LIBS])
])


# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_check_openssl.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_OPENSSL([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for OpenSSL in a number of default spots, or in a user-selected
#   spot (via --with-openssl).  Sets
#
#     OPENSSL_INCLUDES to the include directives required
#     OPENSSL_LIBS to the -l directives required
#     OPENSSL_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets OPENSSL_INCLUDES such that source files should use the
#   openssl/ directory in include directives:
#
#     #include <openssl/hmac.h>
#
# LICENSE
#
#   Copyright (c) 2009,2010 Zmanda Inc. <http://www.zmanda.com/>
#   Copyright (c) 2009,2010 Dustin J. Mitchell <dustin@zmanda.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 8

AU_ALIAS([CHECK_SSL], [AX_CHECK_OPENSSL])
AC_DEFUN([AX_CHECK_OPENSSL], [
    found=false
    AC_ARG_WITH([openssl],
        [AS_HELP_STRING([--with-openssl=DIR],
            [root of the OpenSSL directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-openssl value])
              ;;
            *) ssldirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and openssl has installed a .pc file,
            # then use that information and don't search ssldirs
            AC_PATH_PROG([PKG_CONFIG], [pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                OPENSSL_LDFLAGS=`$PKG_CONFIG openssl --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    OPENSSL_LIBS=`$PKG_CONFIG openssl --libs-only-l 2>/dev/null`
                    OPENSSL_INCLUDES=`$PKG_CONFIG openssl --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default ssldirs
            if ! $found; then
                ssldirs="/usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr"
            fi
        ]
        )


    # note that we #include <openssl/foo.h>, so the OpenSSL headers have to be in
    # an 'openssl' subdirectory

    if ! $found; then
        OPENSSL_INCLUDES=
        for ssldir in $ssldirs; do
            AC_MSG_CHECKING([for openssl/ssl.h in $ssldir])
            if test -f "$ssldir/include/openssl/ssl.h"; then
                OPENSSL_INCLUDES="-I$ssldir/include"
                OPENSSL_LDFLAGS="-L$ssldir/lib"
                OPENSSL_LIBS="-lssl -lcrypto"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done

        # if the file wasn't found, well, go ahead and try the link anyway -- maybe
        # it will just work!
    fi

    # try the preprocessor and linker with our new flags,
    # being careful not to pollute the global LIBS, LDFLAGS, and CPPFLAGS

    AC_MSG_CHECKING([whether compiling and linking against OpenSSL works])
    echo "Trying link with OPENSSL_LDFLAGS=$OPENSSL_LDFLAGS;" \
        "OPENSSL_LIBS=$OPENSSL_LIBS; OPENSSL_INCLUDES=$OPENSSL_INCLUDES" >&AS_MESSAGE_LOG_FD

    save_LIBS="$LIBS"
    save_LDFLAGS="$LDFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    LDFLAGS="$LDFLAGS $OPENSSL_LDFLAGS"
    LIBS="$OPENSSL_LIBS $LIBS"
    CPPFLAGS="$OPENSSL_INCLUDES $CPPFLAGS"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <openssl/ssl.h>], [SSL_new(NULL)])],
        [
            AC_MSG_RESULT([yes])
            $1
        ], [
            AC_MSG_RESULT([no])
            $2
        ])
    CPPFLAGS="$save_CPPFLAGS"
    LDFLAGS="$save_LDFLAGS"
    LIBS="$save_LIBS"

    AC_SUBST([OPENSSL_INCLUDES])
    AC_SUBST([OPENSSL_LIBS])
    AC_SUBST([OPENSSL_LDFLAGS])
])
# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_path_lib_pcre.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PATH_LIB_PCRE [(A/NA)]
#
# DESCRIPTION
#
#   check for pcre lib and set PCRE_LIBS and PCRE_CFLAGS accordingly.
#
#   also provide --with-pcre option that may point to the $prefix of the
#   pcre installation - the macro will check $pcre/include and $pcre/lib to
#   contain the necessary files.
#
#   the usual two ACTION-IF-FOUND / ACTION-IF-NOT-FOUND are supported and
#   they can take advantage of the LIBS/CFLAGS additions.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 7

AC_DEFUN([AX_PATH_LIB_PCRE],[dnl
AC_MSG_CHECKING([lib pcre])
AC_ARG_WITH(pcre,
[  --with-pcre[[=prefix]]    compile xmlpcre part (via libpcre check)],,
     with_pcre="yes")
if test ".$with_pcre" = ".no" ; then
  AC_MSG_RESULT([disabled])
  m4_ifval($2,$2)
else
  AC_MSG_RESULT([(testing)])
  AC_CHECK_LIB(pcre, pcre_study)
  if test "$ac_cv_lib_pcre_pcre_study" = "yes" ; then
     PCRE_LIBS="-lpcre"
     AC_MSG_CHECKING([lib pcre])
     AC_MSG_RESULT([$PCRE_LIBS])
     m4_ifval($1,$1)
  else
     OLDLDFLAGS="$LDFLAGS" ; LDFLAGS="$LDFLAGS -L$with_pcre/lib"
     OLDCPPFLAGS="$CPPFLAGS" ; CPPFLAGS="$CPPFLAGS -I$with_pcre/include"
     AC_CHECK_LIB(pcre, pcre_compile)
     CPPFLAGS="$OLDCPPFLAGS"
     LDFLAGS="$OLDLDFLAGS"
     if test "$ac_cv_lib_pcre_pcre_compile" = "yes" ; then
        AC_MSG_RESULT(.setting PCRE_LIBS -L$with_pcre/lib -lpcre)
        PCRE_LIBS="-L$with_pcre/lib -lpcre"
        test -d "$with_pcre/include" && PCRE_CFLAGS="-I$with_pcre/include"
        AC_MSG_CHECKING([lib pcre])
        AC_MSG_RESULT([$PCRE_LIBS])
        m4_ifval($1,$1)
     else
        AC_MSG_CHECKING([lib pcre])
        AC_MSG_RESULT([no, (WARNING)])
        m4_ifval($2,$2)
     fi
  fi
fi
AC_SUBST([PCRE_LIBS])
AC_SUBST([PCRE_CFLAGS])
])
# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_type_socklen_t.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_TYPE_SOCKLEN_T
#
# DESCRIPTION
#
#   Check whether sys/socket.h defines type socklen_t. Please note that some
#   systems require sys/types.h to be included before sys/socket.h can be
#   compiled.
#
# LICENSE
#
#   Copyright (c) 2008 Lars Brinkhoff <lars@nocrew.org>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 5

AU_ALIAS([TYPE_SOCKLEN_T], [AX_TYPE_SOCKLEN_T])
AC_DEFUN([AX_TYPE_SOCKLEN_T],
[AC_CACHE_CHECK([for socklen_t], ac_cv_ax_type_socklen_t,
[
  AC_TRY_COMPILE(
  [#include <sys/types.h>
   #include <sys/socket.h>],
  [socklen_t len = 42; return 0;],
  ac_cv_ax_type_socklen_t=yes,
  ac_cv_ax_type_socklen_t=no)
])
  if test $ac_cv_ax_type_socklen_t != yes; then
    AC_DEFINE(socklen_t, int, [Substitute for socklen_t])
  fi
])
# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_lib_socket_nsl.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_SOCKET_NSL
#
# DESCRIPTION
#
#   This macro figures out what libraries are required on this platform to
#   link sockets programs.
#
#   The common cases are not to need any extra libraries, or to need
#   -lsocket and -lnsl. We need to avoid linking with libnsl unless we need
#   it, though, since on some OSes where it isn't necessary it will totally
#   break networking. Unisys also includes gethostbyname() in libsocket but
#   needs libnsl for socket().
#
# LICENSE
#
#   Copyright (c) 2008 Russ Allbery <rra@stanford.edu>
#   Copyright (c) 2008 Stepan Kasal <kasal@ucw.cz>
#   Copyright (c) 2008 Warren Young <warren@etr-usa.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AU_ALIAS([LIB_SOCKET_NSL], [AX_LIB_SOCKET_NSL])
AC_DEFUN([AX_LIB_SOCKET_NSL],
[
	AC_SEARCH_LIBS([gethostbyname], [nsl])
	AC_SEARCH_LIBS([socket], [socket], [], [
		AC_CHECK_LIB([socket], [socket], [LIBS="-lsocket -lnsl $LIBS"],
		[], [-lnsl])])
])
# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_func_snprintf.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_FUNC_SNPRINTF
#
# DESCRIPTION
#
#   Checks for a fully C99 compliant snprintf, in particular checks whether
#   it does bounds checking and returns the correct string length; does the
#   same check for vsnprintf. If no working snprintf or vsnprintf is found,
#   request a replacement and warn the user about it. Note: the mentioned
#   replacement is freely available and may be used in any project
#   regardless of it's license.
#
# LICENSE
#
#   Copyright (c) 2008 Ruediger Kuhlmann <info@ruediger-kuhlmann.de>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 5

AU_ALIAS([AC_FUNC_SNPRINTF], [AX_FUNC_SNPRINTF])
AC_DEFUN([AX_FUNC_SNPRINTF],
[AC_CHECK_FUNCS(snprintf vsnprintf)
AC_MSG_CHECKING(for working snprintf)
AC_CACHE_VAL(ac_cv_have_working_snprintf,
[AC_TRY_RUN(
[#include <stdio.h>

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = snprintf (bufs, 2, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = snprintf (bufd, 2, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_snprintf=yes, ac_cv_have_working_snprintf=no, ac_cv_have_working_snprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_snprintf])
AC_MSG_CHECKING(for working vsnprintf)
AC_CACHE_VAL(ac_cv_have_working_vsnprintf,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdarg.h>

int my_vsnprintf (char *buf, const char *tmpl, ...)
{
    int i;
    va_list args;
    va_start (args, tmpl);
    i = vsnprintf (buf, 2, tmpl, args);
    va_end (args);
    return i;
}

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = my_vsnprintf (bufs, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = my_vsnprintf (bufd, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_vsnprintf=yes, ac_cv_have_working_vsnprintf=no, ac_cv_have_working_vsnprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_vsnprintf])
if test x$ac_cv_have_working_snprintf$ac_cv_have_working_vsnprintf != "xyesyes"; then
  AC_LIBOBJ(snprintf)
  AC_MSG_WARN([Replacing missing/broken (v)snprintf() with version from http://www.ijs.si/software/snprintf/.])
  AC_DEFINE(PREFER_PORTABLE_SNPRINTF, 1, "enable replacement (v)snprintf if system (v)snprintf is broken")
fi])
# ===========================================================================
#    http://www.gnu.org/software/autoconf-archive/ax_c___attribute__.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_C___ATTRIBUTE__
#
# DESCRIPTION
#
#   Provides a test for the compiler support of __attribute__ extensions.
#   Defines HAVE___ATTRIBUTE__ if it is found.
#
# LICENSE
#
#   Copyright (c) 2008 Stepan Kasal <skasal@redhat.com>
#   Copyright (c) 2008 Christian Haggstrom
#   Copyright (c) 2008 Ryan McCabe <ryan@numb.org>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 8

AC_DEFUN([AX_C___ATTRIBUTE__], [
  AC_CACHE_CHECK([for __attribute__], [ax_cv___attribute__],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
	[[#include <stdlib.h>
	  static void foo(void) __attribute__ ((unused));
	  static void
	  foo(void) {
	      exit(1);
	  }
        ]], [])],
      [ax_cv___attribute__=yes],
      [ax_cv___attribute__=no]
    )
  ])
  if test "$ax_cv___attribute__" = "yes"; then
    AC_DEFINE([HAVE___ATTRIBUTE__], 1, [define if your compiler has __attribute__])
  fi
])
# ===========================================================================
#    http://www.gnu.org/software/autoconf-archive/ax_gcc_malloc_call.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_GCC_MALLOC_CALL
#
# DESCRIPTION
#
#   The macro will compile a test program to see whether the compiler does
#   understand the per-function postfix pragma.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 7

AC_DEFUN([AX_GCC_MALLOC_CALL],[dnl
AC_CACHE_CHECK(
 [whether the compiler supports function __attribute__((__malloc__))],
 ax_cv_gcc_malloc_call,[
 AC_TRY_COMPILE([__attribute__((__malloc__))
 int f(int i) { return i; }],
 [],
 ax_cv_gcc_malloc_call=yes, ax_cv_gcc_malloc_call=no)])
 if test "$ax_cv_gcc_malloc_call" = yes; then
   AC_DEFINE([GCC_MALLOC_CALL],[__attribute__((__malloc__))],
    [most gcc compilers know a function __attribute__((__malloc__))])
 fi
])
# ===========================================================================
#        http://www.gnu.org/software/autoconf-archive/ax_zoneinfo.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_ZONEINFO([options...])
#
# DESCRIPTION
#
#   This macro finds compiled zoneinfo files.  If successful it will define
#   HAVE_ZONEINFO per:
#
#     AC_DEFINE([HAVE_ZONEINFO], [1], [...])
#
#   and have the variable TZDIR point to the zoneinfo directory as per
#
#     AC_SUBST([TZDIR])
#     AC_DEFINE_UNQUOTED([TZDIR], [/path/to/zic/files], [...])
#
#   Optionally, OPTIONS can be `right' to trigger further tests that will
#   determine if leap second fix-ups are available.  If so the variables
#   HAVE_ZONEINFO_RIGHT, ZONEINFO_UTC_RIGHT and TZDIR_RIGHT will be
#   populated:
#
#     AC_DEFINE([HAVE_ZONEINFO_RIGHT], [1], [...])
#     AC_SUBST([TZDIR_RIGHT])
#     AC_DEFINE_UNQUOTED([TZDIR_RIGHT], [/path/to/right/zic/files], [...])
#     AC_SUBST([ZONEINFO_UTC_RIGHT])
#     AC_DEFINE_UNQUOTED([ZONEINFO_UTC_RIGHT], [$ZONEINFO_UTC_RIGHT], [...])
#
# LICENSE
#
#   Copyright (c) 2012 Sebastian Freundt <freundt@fresse.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 3

AC_DEFUN([AX_ZONEINFO_TZFILE_H], [dnl
	dnl not totally necessary (yet), as we can simply inspect the tzfiles
	dnl ourselves, but it certainly helps
	AC_CHECK_HEADER([tzfile.h])
])dnl AX_ZONEINFO_TZFILE_H

AC_DEFUN([AX_ZONEINFO_CHECK_TZFILE], [dnl
	dnl AX_ZONEINFO_CHECK_TZFILE([FILE], [ACTION-IF-VALID], [ACTION-IF-NOT])
	dnl secret switch is the 4th argument, which determines the ret code
	dnl of the leapcnt check
	pushdef([probe], [$1])
	pushdef([if_found], [$2])
	pushdef([if_not_found], [$3])

	AC_REQUIRE([AX_ZONEINFO_TZFILE_H])

	if test -z "${ax_tmp_zoneinfo_nested}"; then
		AC_MSG_CHECKING([zoneinfo file ]probe[])
	fi

	AC_LANG_PUSH([C])
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

]]ifelse([$4], [], [], [[
#define CHECK_LEAPCNT	]]$4[[
]])[[

/* simplified struct */
struct tzhead {
	char	tzh_magic[4];		/* TZ_MAGIC */
	char	tzh_version[1];		/* '\0' or '2' as of 2005 */
	char	tzh_reserved[15];	/* reserved--must be zero */
	char	tzh_ttisgmtcnt[4];	/* coded number of trans. time flags */
	char	tzh_ttisstdcnt[4];	/* coded number of trans. time flags */
	char	tzh_leapcnt[4];		/* coded number of leap seconds */
	char	tzh_timecnt[4];		/* coded number of transition times */
	char	tzh_typecnt[4];		/* coded number of local time types */
	char	tzh_charcnt[4];		/* coded number of abbr. chars */
};

int
main(int argc, char *argv[])
{
	struct tzhead foo;
	int f;

	if (argc <= 1) {
		return 0;
	} else if ((f = open(argv[1], O_RDONLY, 0644)) < 0) {
		return 1;
	} else if (read(f, &foo, sizeof(foo)) != sizeof(foo)) {
		return 1;
	} else if (close(f) < 0) {
		return 1;
	}

	/* inspect the header */
	if (memcmp(foo.tzh_magic, "TZif", sizeof(foo.tzh_magic))) {
		return 1;
	} else if (*foo.tzh_version && *foo.tzh_version != '2') {
		return 1;
#if defined CHECK_LEAPCNT
	} else if (!foo.tzh_leapcnt[0] && !foo.tzh_leapcnt[1] &&
		   !foo.tzh_leapcnt[2] && !foo.tzh_leapcnt[3]) {
		return CHECK_LEAPCNT;
#endif  /* CHECK_LEAPCNT */
	}

	/* otherwise everything's in order */
	return 0;
}
]])], [## call the whole shebang again with the tzfile
		if ./conftest$EXEEXT probe; then
			if test -z "${ax_tmp_zoneinfo_nested}"; then
				AC_MSG_RESULT([looking good])
			fi
			[]if_found[]
		else
			if test -z "${ax_tmp_zoneinfo_nested}"; then
				AC_MSG_RESULT([looking bad ${ax_tmp_rc}])
			fi
			[]if_not_found[]
		fi
], [
		if test -z "${ax_tmp_zoneinfo_nested}"; then
			AC_MSG_RESULT([impossible])
		fi
		[]if_not_found[]])
	AC_LANG_POP([C])

	popdef([probe])
	popdef([if_found])
	popdef([if_not_found])
])dnl AX_ZONEINFO_CHECK_TZFILE

AC_DEFUN([AX_ZONEINFO_TZDIR], [dnl
	dnl we consider a zoneinfo directory properly populated when it
	dnl provides UTC or UCT or Universal or Zulu

	pushdef([check_tzdir], [dnl
		pushdef([dir], $]1[)dnl
		test -n []dir[] && test -d []dir[] dnl
		popdef([dir])dnl
	])dnl check_tzdir

	dnl try /etc/localtime first, sometimes it's a link into TZDIR
	if test -L "/etc/localtime"; then
		TZDIR_cand="`readlink /etc/localtime` ${TZDIR_cand}"
	fi

	dnl oh, how about we try and check if there is a TZDIR already
	if check_tzdir(["${TZDIR}"]); then
		## bingo
		TZDIR_cand="${TZDIR} ${TZDIR_cand}"
	fi

	dnl often there's a tzselect util which contains the TZDIR path
	AC_PATH_PROG([TZSELECT], [tzselect])
	if test -n "${ac_cv_path_TZSELECT}"; then
		dnl snarf the value
		valtmp="`mktemp`"
		strings "${ac_cv_path_TZSELECT}" | \
			grep -F 'TZDIR=' > "${valtmp}"
		. "${valtmp}"
		TZDIR_cand="${TZDIR} ${TZDIR_cand}"
		rm -f -- "${valtmp}"
	fi

	dnl lastly, append the usual suspects
	TZDIR_cand="${TZDIR_cand} \
/usr/share/zoneinfo \
/usr/lib/zoneinfo \
/usr/local/etc/zoneinfo \
/usr/share/lib/zoneinfo \
"

	dnl go through our candidates
	AC_CACHE_CHECK([for TZDIR], [ax_cv_zoneinfo_tzdir], [dnl
		ax_tmp_zoneinfo_nested="yes"
		for c in ${TZDIR_cand}; do
			ax_cv_zoneinfo_utc=""
			for f in "UTC" "UCT" "Universal" "Zulu"; do
				AX_ZONEINFO_CHECK_TZFILE(["${c}/${f}"], [
					dnl ACTION-IF-FOUND
					ax_cv_zoneinfo_utc="${c}/${f}"
					break
				])
			done
			if test -n "${ax_cv_zoneinfo_utc}"; then
				ax_cv_zoneinfo_tzdir="${c}"
				break
			fi
		done
		ax_tmp_zoneinfo_nested=""
	])dnl ax_cv_tzdir

	TZDIR="${ax_cv_zoneinfo_tzdir}"
	AC_SUBST([TZDIR])

	if check_tzdir(["${ax_cv_zoneinfo_tzdir}"]); then
		AC_DEFINE([HAVE_ZONEINFO], [1], [dnl
Define when zoneinfo directory has been present during configuration.])
		AC_DEFINE_UNQUOTED([TZDIR], ["${ax_cv_zoneinfo_tzdir}"], [
Configuration time zoneinfo directory.])
	fi

	popdef([check_tzdir])
])dnl AX_ZONEINFO_TZDIR

AC_DEFUN([AX_ZONEINFO_RIGHT], [dnl
	AC_REQUIRE([AX_ZONEINFO_TZDIR])

	TZDIR_cand="${TZDIR} \
${TZDIR}/leapseconds \
${TZDIR}-leaps \
${TZDIR}/right \
${TZDIR}-posix \
${TZDIR}/posix \
"

	dnl go through our candidates
	AC_CACHE_CHECK([for leap second file], [ax_cv_zoneinfo_utc_right], [dnl
		ax_tmp_zoneinfo_nested="yes"
		if test -n "${ax_cv_zoneinfo_utc}"; then
			__utc_file="`basename "${ax_cv_zoneinfo_utc}"`"
			for c in ${TZDIR_cand}; do
				if test -d "${c}"; then
					c="${c}/${__utc_file}"
				fi
				AX_ZONEINFO_CHECK_TZFILE(["${c}"], [
					dnl ACTION-IF-FOUND
					ax_cv_zoneinfo_utc_right="${c}"
					break
				], [:], [2])
			done
		fi
		ax_tmp_zoneinfo_nested=""
	])dnl ax_cv_tzdir

	ZONEINFO_UTC_RIGHT="${ax_cv_zoneinfo_utc_right}"
	AC_SUBST([ZONEINFO_UTC_RIGHT])
	AC_SUBST([TZDIR_RIGHT])

	if test -n "${ax_cv_zoneinfo_utc_right}"; then
		TZDIR_RIGHT="`dirname ${ax_cv_zoneinfo_utc_right}`"

		AC_DEFINE([HAVE_ZONEINFO_RIGHT], [1], [dnl
Define when zoneinfo directory has been present during configuration.])
		AC_DEFINE_UNQUOTED([TZDIR_RIGHT],
			["${TZDIR_RIGHT}"], [
Configuration time zoneinfo directory.])
		AC_DEFINE_UNQUOTED([ZONEINFO_UTC_RIGHT],
			["${ax_cv_zoneinfo_utc_right}"], [
Leap-second aware UTC zoneinfo file.])
	fi
])dnl AX_ZONEINFO_RIGHT

AC_DEFUN([AX_ZONEINFO], [
	AC_REQUIRE([AX_ZONEINFO_TZDIR])

	ifelse([$1], [right], [
		AC_REQUIRE([AX_ZONEINFO_RIGHT])
	])

	AC_ARG_VAR([TZDIR], [Directory with compiled zoneinfo files.])
])dnl AX_ZONEINFO

dnl ax_zoneinfo.m4 ends here

dnl The following no longer appear to be part of autoconf-archive.
dnl TODO: Look into replacing them with something else
.# ===========================================================================
#             http://autoconf-archive.cryp.to/ax_gcc_option.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_GCC_OPTION(OPTION,EXTRA-OPTIONS,TEST-PROGRAM,ACTION-IF-SUCCESSFUL,ACTION-IF-NOT-SUCCESFUL)
#
# DESCRIPTION
#
#   AX_GCC_OPTION checks wheter gcc accepts the passed OPTION. If it accepts
#   the OPTION then ACTION-IF-SUCCESSFUL will be executed, otherwise
#   ACTION-IF-UNSUCCESSFUL.
#
#   NOTE: This macro will be obsoleted by AX_C_CHECK_FLAG AX_CXX_CHECK_FLAG,
#   AX_CPP_CHECK_FLAG, AX_CXXCPP_CHECK_FLAG and AX_LD_CHECK_FLAG.
#
#   A typical usage should be the following one:
#
#     AX_GCC_OPTION([-fomit-frame-pointer],[],[],[
#       AC_MSG_NOTICE([The option is supported])],[
#       AC_MSG_NOTICE([No luck this time])
#     ])
#
#   The macro doesn't discriminate between languages so, if you are testing
#   for an option that works for C++ but not for C you should use '-x c++'
#   as EXTRA-OPTIONS:
#
#     AX_GCC_OPTION([-fno-rtti],[-x c++],[],[ ... ],[ ... ])
#
#   OPTION is tested against the following code:
#
#     int main()
#     {
#             return 0;
#     }
#
#   The optional TEST-PROGRAM comes handy when the default main() is not
#   suited for the option being checked
#
#   So, if you need to test for -fstrict-prototypes option you should
#   probably use the macro as follows:
#
#     AX_GCC_OPTION([-fstrict-prototypes],[-x c++],[
#       int main(int argc, char ** argv)
#       {
#       	(void) argc;
#       	(void) argv;
#
#       	return 0;
#       }
#     ],[ ... ],[ ... ])
#
#   Note that the macro compiles but doesn't link the test program so it is
#   not suited for checking options that are passed to the linker, like:
#
#     -Wl,-L<a-library-path>
#
#   In order to avoid such kind of problems you should think about usinguse
#   the AX_*_CHECK_FLAG family macros
#
# LAST MODIFICATION
#
#   2008-04-12
#
# COPYLEFT
#
#   Copyright (c) 2008 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#   Copyright (c) 2008 Bogdan Drozdowski <bogdandr@op.pl>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Macro Archive. When you make and
#   distribute a modified version of the Autoconf Macro, you may extend this
#   special exception to the GPL to apply to your modified version as well.

AC_DEFUN([AX_GCC_OPTION], [
  AC_REQUIRE([AC_PROG_CC])

  AC_MSG_CHECKING([if gcc accepts $1 option])

  AS_IF([ test "x$GCC" = "xyes" ],[
    AS_IF([ test -z "$3" ],[
      ax_gcc_option_test="int main()
{
	return 0;
}"
    ],[
      ax_gcc_option_test="$3"
    ])

    # Dump the test program to file
    cat <<EOF > conftest.c
$ax_gcc_option_test
EOF

    # Dump back the file to the log, useful for debugging purposes
    AC_TRY_COMMAND(cat conftest.c 1>&AS_MESSAGE_LOG_FD)

    AS_IF([ AC_TRY_COMMAND($CC $2 $1 -c conftest.c 1>&AS_MESSAGE_LOG_FD) ],[
   	        AC_MSG_RESULT([yes])
    	$4
    ],[
   		AC_MSG_RESULT([no])
    	$5
    ])
  ],[
    AC_MSG_RESULT([no gcc available])
  ])
])

# ===========================================================================
#            http://autoconf-archive.cryp.to/ax_ld_check_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LD_CHECK_FLAG(FLAG-TO-CHECK,[PROLOGUE],[BODY],[ACTION-IF-SUCCESS],[ACTION-IF-FAILURE])
#
# DESCRIPTION
#
#   This macro tests if the C++ compiler supports the flag FLAG-TO-CHECK. If
#   successfull execute ACTION-IF-SUCCESS otherwise ACTION-IF-FAILURE.
#   PROLOGUE and BODY are optional and should be used as in AC_LANG_PROGRAM
#   macro.
#
#   Example:
#
#     AX_LD_CHECK_FLAG([-Wl,-L/usr/lib],[],[],[
#       ...
#     ],[
#       ...
#     ])
#
#   This code is inspired from KDE_CHECK_COMPILER_FLAG macro. Thanks to
#   Bogdan Drozdowski <bogdandr@op.pl> for testing and bug fixes.
#
# LAST MODIFICATION
#
#   2008-04-12
#
# COPYLEFT
#
#   Copyright (c) 2008 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Macro Archive. When you make and
#   distribute a modified version of the Autoconf Macro, you may extend this
#   special exception to the GPL to apply to your modified version as well.

AC_DEFUN([AX_LD_CHECK_FLAG],[
  AC_PREREQ([2.61])
  AC_REQUIRE([AC_PROG_CXX])
  AC_REQUIRE([AC_PROG_SED])

  flag=`echo "$1" | $SED 'y% .=/+-(){}<>:*,%_______________%'`

  AC_CACHE_CHECK([whether the linker accepts the $1 flag],
    [ax_cv_ld_check_flag_$flag],[

    #AC_LANG_PUSH([C])

    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $1"
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([$2],[$3])
    ],[
      eval "ax_cv_ld_check_flag_$flag=yes"
    ],[
      eval "ax_cv_ld_check_flag_$flag=no"
    ])

    LDFLAGS="$save_LDFLAGS"

    #AC_LANG_POP

  ])

  AS_IF([eval "test \"`echo '$ax_cv_ld_check_flag_'$flag`\" = yes"],[
    :
    $4
  ],[
    :
    $5
  ])
])

