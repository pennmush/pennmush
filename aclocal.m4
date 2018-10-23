# ===========================================================================
#       https://www.gnu.org/software/autoconf-archive/ax_lib_mysql.html
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

#serial 13

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
#    https://www.gnu.org/software/autoconf-archive/ax_lib_postgresql.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_POSTGRESQL([MINIMUM-VERSION],[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
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
#   can't be found in the PATH (You could set also PG_CONFIG variable)
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
#   It execute if found ACTION-IF-FOUND (empty by default) and
#   ACTION-IF-NOT-FOUND (AC_MSG_FAILURE by default) if not found.
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#   Copyright (c) 2014 Sree Harsha Totakura <sreeharsha@totakura.in>
#   Copyright (c) 2018 Bastien Roucaries <rouca@debian.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 22

AC_DEFUN([_AX_LIB_POSTGRESQL_OLD],[
	found_postgresql="no"
	_AX_LIB_POSTGRESQL_OLD_fail="no"
	while true; do
	  AC_CACHE_CHECK([for the pg_config program], [ac_cv_path_PG_CONFIG],
	    [AC_PATH_PROGS_FEATURE_CHECK([PG_CONFIG], [pg_config],
	      [[ac_cv_path_PG_CONFIG="";$ac_path_PG_CONFIG --includedir > /dev/null \
		&& ac_cv_path_PG_CONFIG=$ac_path_PG_CONFIG ac_path_PG_CONFIG_found=:]],
	      [ac_cv_path_PG_CONFIG=""])])
	  PG_CONFIG=$ac_cv_path_PG_CONFIG
	  AS_IF([test "X$PG_CONFIG" = "X"],[break])

	  AC_CACHE_CHECK([for the PostgreSQL libraries CPPFLAGS],[ac_cv_POSTGRESQL_CPPFLAGS],
		       [ac_cv_POSTGRESQL_CPPFLAGS="-I`$PG_CONFIG --includedir`" || _AX_LIB_POSTGRESQL_OLD_fail=yes])
	  AS_IF([test "X$_AX_LIB_POSTGRESQL_OLD_fail" = "Xyes"],[break])
	  POSTGRESQL_CPPFLAGS="$ac_cv_POSTGRESQL_CPPFLAGS"

	  AC_CACHE_CHECK([for the PostgreSQL libraries LDFLAGS],[ac_cv_POSTGRESQL_LDFLAGS],
		       [ac_cv_POSTGRESQL_LDFLAGS="-L`$PG_CONFIG --libdir`" || _AX_LIB_POSTGRESQL_OLD_fail=yes])
	  AS_IF([test "X$_AX_LIB_POSTGRESQL_OLD_fail" = "Xyes"],[break])
	  POSTGRESQL_LDFLAGS="$ac_cv_POSTGRESQL_LDFLAGS"

	  AC_CACHE_CHECK([for the PostgreSQL libraries LIBS],[ac_cv_POSTGRESQL_LIBS],
		       [ac_cv_POSTGRESQL_LIBS="-lpq"])
	  POSTGRESQL_LIBS="$ac_cv_POSTGRESQL_LIBS"

	  AC_CACHE_CHECK([for the PostgreSQL version],[ac_cv_POSTGRESQL_VERSION],
		       [
			ac_cv_POSTGRESQL_VERSION=`$PG_CONFIG --version | sed "s/^PostgreSQL[[[:space:]]][[[:space:]]]*\([[0-9.]][[0-9.]]*\).*/\1/"` \
			      || _AX_LIB_POSTGRESQL_OLD_fail=yes
		       ])
	  AS_IF([test "X$_AX_LIB_POSTGRESQL_OLD_fail" = "Xyes"],[break])
	  POSTGRESQL_VERSION="$ac_cv_POSTGRESQL_VERSION"


	  dnl
	  dnl Check if required version of PostgreSQL is available
	  dnl
	  AS_IF([test X"$postgresql_version_req" != "X"],[
	     AC_MSG_CHECKING([if PostgreSQL version $POSTGRESQL_VERSION is >= $postgresql_version_req])
	     AX_COMPARE_VERSION([$POSTGRESQL_VERSION],[ge],[$postgresql_version_req],
				[found_postgresql_req_version=yes],[found_postgresql_req_version=no])
	     AC_MSG_RESULT([$found_postgresql_req_version])
	  ])
	  AS_IF([test "Xfound_postgresql_req_version" = "Xno"],[break])

	  found_postgresql="yes"
	  break
	done
])

AC_DEFUN([_AX_LIB_POSTGRESQL_PKG_CONFIG],
[
  AC_REQUIRE([PKG_PROG_PKG_CONFIG])
  found_postgresql=no

  while true; do
    PKG_PROG_PKG_CONFIG
    AS_IF([test X$PKG_CONFIG = X],[break])

    _AX_LIB_POSTGRESQL_PKG_CONFIG_fail=no;
    AS_IF([test "X$postgresql_version_req" = "X"],
	  [PKG_CHECK_EXISTS([libpq],[found_postgresql_pkg_config=yes],[found_postgresql=no])],
	  [PKG_CHECK_EXISTS([libpq >= "$postgresql_version_req"],
			   [found_postgresql=yes],[found_postgresql=no])])
    AS_IF([test "X$found_postgresql" = "no"],[break])

    AC_CACHE_CHECK([for the PostgreSQL libraries CPPFLAGS],[ac_cv_POSTGRESQL_CPPFLAGS],
		   [ac_cv_POSTGRESQL_CPPFLAGS="`$PKG_CONFIG libpq --cflags-only-I`" || _AX_LIB_POSTGRESQL_PKG_CONFIG_fail=yes])
    AS_IF([test "X$_AX_LIB_POSTGRESQL_PKG_CONFIG_fail" = "Xyes"],[break])
    POSTGRESQL_CPPFLAGS="$ac_cv_POSTGRESQL_CPPFLAGS"


    AC_CACHE_CHECK([for the PostgreSQL libraries LDFLAGS],[ac_cv_POSTGRESQL_LDFLAGS],
		   [ac_cv_POSTGRESQL_LDFLAGS="`$PKG_CONFIG libpq --libs-only-L --libs-only-other`" || _AX_LIB_POSTGRESQL_PKG_CONFIG_fail=yes])
    AS_IF([test "X$_AX_LIB_POSTGRESQL_PKG_CONFIG_fail" = "Xyes"],[break])
    POSTGRESQL_LDFLAGS="$ac_cv_POSTGRESQL_LDFLAGS"


    AC_CACHE_CHECK([for the PostgreSQL libraries LIBS],[ac_cv_POSTGRESQL_LIBS],
		   [ac_cv_POSTGRESQL_LIBS="`$PKG_CONFIG libpq --libs-only-l`" || _AX_LIB_POSTGRESQL_PKG_CONFIG_fail=ye])
    AS_IF([test "X$_AX_LIB_POSTGRESQL_PKG_CONFIG_fail" = "Xyes"],[break])
    POSTGRESQL_LIBS="$ac_cv_POSTGRESQL_LIBS"

    dnl already checked by exist but need to be recovered
    AC_CACHE_CHECK([for the PostgreSQL version],[ac_cv_POSTGRESQL_VERSION],
		   [ac_cv_POSTGRESQL_VERSION="`$PKG_CONFIG libpq --modversion`" || _AX_LIB_POSTGRESQL_PKG_CONFIG_fail=yes])
    AS_IF([test "X$_AX_LIB_POSTGRESQL_PKG_CONFIG_fail" = "Xyes"],[break])
    POSTGRESQL_VERSION="$ac_cv_POSTGRESQL_VERSION"

    found_postgresql=yes
    break;
  done

])



AC_DEFUN([AX_LIB_POSTGRESQL],
[
    AC_ARG_WITH([postgresql],
	AS_HELP_STRING([--with-postgresql=@<:@ARG@:>@],
	    [use PostgreSQL library @<:@default=yes@:>@, optionally specify path to pg_config]
	),
	[
	AS_CASE([$withval],
		[[[nN]][[oO]]],[want_postgresql="no"],
		[[[yY]][[eE]][[sS]]],[want_postgresql="yes"],
		[
			want_postgresql="yes"
			PG_CONFIG="$withval"
		])
	],
	[want_postgresql="yes"]
    )

    AC_ARG_VAR([POSTGRESQL_CPPFLAGS],[cpp flags for PostgreSQL overriding detected flags])
    AC_ARG_VAR([POSTGRESQL_LIBFLAGS],[libs for PostgreSQL overriding detected flags])
    AC_ARG_VAR([POSTGRESQL_LDFLAGS],[linker flags for PostgreSQL overriding detected flags])

    # populate cache
    AS_IF([test "X$POSTGRESQL_CPPFLAGS" != X],[ac_cv_POSTGRESQL_CPPFLAGS="$POSTGRESQL_CPPFLAGS"])
    AS_IF([test "X$POSTGRESQL_LDFLAGS" != X],[ac_cv_POSTGRESQL_LDFLAGS="$POSTGRESQL_LDFLAGS"])
    AS_IF([test "X$POSTGRESQL_LIBS" != X],[ac_cv_POSTGRESQL_LIBS="$POSTGRESQL_LIBS"])

    postgresql_version_req=ifelse([$1], [], [], [$1])
    found_postgresql="no"

    POSTGRESQL_VERSION=""

    dnl
    dnl Check PostgreSQL libraries (libpq)
    dnl
    AS_IF([test X"$want_postgresql" = "Xyes"],[
      _AX_LIB_POSTGRESQL_PKG_CONFIG


      AS_IF([test X"$found_postgresql" = "Xno"],
	    [_AX_LIB_POSTGRESQL_OLD])

      AS_IF([test X"$found_postgresql" = Xyes],[
	  _AX_LIB_POSTGRESQL_OLD_CPPFLAGS="$CPPFLAGS"
	  CPPFLAGS="$CPPFLAGS $POSTGRESQL_CPPFLAGS"
	  _AX_LIB_POSTGRESQL_OLD_LDFLAGS="$LDFLAGS"
	  LDFLAGS="$LDFLAGS $POSTGRESQL_LDFLAGS"
	  _AX_LIB_POSTGRESQL_OLD_LIBS="$LIBS"
	  LIBS="$LIBS $POSTGRESQL_LIBS"
	  while true; do
	    dnl try to compile
	    AC_CHECK_HEADER([libpq-fe.h],[],[found_postgresql=no])
	    AS_IF([test "X$found_postgresql" = "Xno"],[break])
	    dnl try now to link
	    AC_CACHE_CHECK([for the PostgreSQL library linking is working],[ac_cv_postgresql_found],
	    [
	      AC_LINK_IFELSE([
		AC_LANG_PROGRAM(
		  [
		   #include <libpq-fe.h>
		  ],
		  [[
		    char conninfo[]="dbname = postgres";
		    PGconn     *conn;
		    conn = PQconnectdb(conninfo);
		  ]]
		 )
		],[ac_cv_postgresql_found=yes],
		  [ac_cv_postgresql_found=no])
	     ])
	    found_postgresql="$ac_cv_postgresql_found"
	    AS_IF([test "X$found_postgresql" = "Xno"],[break])
	    break
	done
	CPPFLAGS="$_AX_LIB_POSTGRESQL_OLD_CPPFLAGS"
	LDFLAGS="$_AX_LIB_POSTGRESQL_OLD_LDFLAGS"
	LIBS="$_AX_LIB_POSTGRESQL_OLD_LIBS"
	])


      AS_IF([test "x$found_postgresql" = "xyes"],[
		AC_DEFINE([HAVE_POSTGRESQL], [1],
			  [Define to 1 if PostgreSQL libraries are available])])
    ])

    AC_SUBST([POSTGRESQL_VERSION])
    AC_SUBST([POSTGRESQL_CPPFLAGS])
    AC_SUBST([POSTGRESQL_LDFLAGS])
    AC_SUBST([POSTGRESQL_LIBS])

    AS_IF([test "x$found_postgresql" = "xyes"],
     [ifelse([$2], , :, [$2])],
     [ifelse([$3], , AS_IF([test X"$want_postgresql" = "Xyes"],[AC_MSG_ERROR([Library requirements (PostgreSQL) not met.])],[:]), [$3])])

])

# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_check_openssl.html
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

#serial 10

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
            AC_CHECK_TOOL([PKG_CONFIG], [pkg-config])
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
#    https://www.gnu.org/software/autoconf-archive/ax_type_socklen_t.html
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
#   with this program. If not, see <https://www.gnu.org/licenses/>.
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

AU_ALIAS([TYPE_SOCKLEN_T], [AX_TYPE_SOCKLEN_T])
AC_DEFUN([AX_TYPE_SOCKLEN_T],
[AC_CACHE_CHECK([for socklen_t], [ac_cv_ax_type_socklen_t],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
  #include <sys/socket.h>]],
  [[socklen_t len = (socklen_t) 42; return (!len);]])],
  [ac_cv_ax_type_socklen_t=yes],
  [ac_cv_ax_type_socklen_t=no])
])
  if test $ac_cv_ax_type_socklen_t != yes; then
    AC_DEFINE(socklen_t, int, [Substitute for socklen_t])
  fi
])

# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_lib_socket_nsl.html
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

#serial 7

AU_ALIAS([LIB_SOCKET_NSL], [AX_LIB_SOCKET_NSL])
AC_DEFUN([AX_LIB_SOCKET_NSL],
[
	AC_SEARCH_LIBS([gethostbyname], [nsl])
	AC_SEARCH_LIBS([socket], [socket], [], [
		AC_CHECK_LIB([socket], [socket], [LIBS="-lsocket -lnsl $LIBS"],
		[], [-lnsl])])
])

# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_func_snprintf.html
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

#serial 7

AU_ALIAS([AC_FUNC_SNPRINTF], [AX_FUNC_SNPRINTF])
AC_DEFUN([AX_FUNC_SNPRINTF],
[AC_CHECK_FUNCS(snprintf vsnprintf)
AC_MSG_CHECKING(for working snprintf)
AC_CACHE_VAL(ac_cv_have_working_snprintf,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdio.h>

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
}]])],[ac_cv_have_working_snprintf=yes],[ac_cv_have_working_snprintf=no],[ac_cv_have_working_snprintf=cross])])
AC_MSG_RESULT([$ac_cv_have_working_snprintf])
AC_MSG_CHECKING(for working vsnprintf)
AC_CACHE_VAL(ac_cv_have_working_vsnprintf,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdio.h>
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
}]])],[ac_cv_have_working_vsnprintf=yes],[ac_cv_have_working_vsnprintf=no],[ac_cv_have_working_vsnprintf=cross])])
AC_MSG_RESULT([$ac_cv_have_working_vsnprintf])
if test x$ac_cv_have_working_snprintf$ac_cv_have_working_vsnprintf != "xyesyes"; then
  AC_LIBOBJ(snprintf)
  AC_MSG_WARN([Replacing missing/broken (v)snprintf() with version from http://www.ijs.si/software/snprintf/.])
  AC_DEFINE(PREFER_PORTABLE_SNPRINTF, 1, "enable replacement (v)snprintf if system (v)snprintf is broken")
fi])

# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_c___attribute__.html
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
#   with this program. If not, see <https://www.gnu.org/licenses/>.
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

#serial 9

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
#    https://www.gnu.org/software/autoconf-archive/ax_gcc_malloc_call.html
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
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 10

AC_DEFUN([AX_GCC_MALLOC_CALL],[dnl
AC_CACHE_CHECK(
 [whether the compiler supports function __attribute__((__malloc__))],
 ax_cv_gcc_malloc_call,[
 AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[__attribute__((__malloc__))
 int f(int i) { return i; }]],
 [])],
 [ax_cv_gcc_malloc_call=yes], [ax_cv_gcc_malloc_call=no])])
 if test "$ax_cv_gcc_malloc_call" = yes; then
   AC_DEFINE([GCC_MALLOC_CALL],[__attribute__((__malloc__))],
    [most gcc compilers know a function __attribute__((__malloc__))])
 fi
])

# ===========================================================================
#       https://www.gnu.org/software/autoconf-archive/ax_zoneinfo.html
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

#serial 4

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

# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_check_link_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_LINK_FLAG(FLAG, [ACTION-SUCCESS], [ACTION-FAILURE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   Check whether the given FLAG works with the linker or gives an error.
#   (Warnings, however, are ignored)
#
#   ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
#   success/failure.
#
#   If EXTRA-FLAGS is defined, it is added to the linker's default flags
#   when the check is done.  The check is thus made with the flags: "LDFLAGS
#   EXTRA-FLAGS FLAG".  This can for example be used to force the linker to
#   issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_LINK_IFELSE.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION. Please keep this
#   macro in sync with AX_CHECK_{PREPROC,COMPILE}_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_CHECK_LINK_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_ldflags_$4_$1])dnl
AC_CACHE_CHECK([whether the linker accepts $1], CACHEVAR, [
  ax_check_save_flags=$LDFLAGS
  LDFLAGS="$LDFLAGS $4 $1"
  AC_LINK_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  LDFLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_LINK_FLAGS

# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_compare_version.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_COMPARE_VERSION(VERSION_A, OP, VERSION_B, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
#
# DESCRIPTION
#
#   This macro compares two version strings. Due to the various number of
#   minor-version numbers that can exist, and the fact that string
#   comparisons are not compatible with numeric comparisons, this is not
#   necessarily trivial to do in a autoconf script. This macro makes doing
#   these comparisons easy.
#
#   The six basic comparisons are available, as well as checking equality
#   limited to a certain number of minor-version levels.
#
#   The operator OP determines what type of comparison to do, and can be one
#   of:
#
#    eq  - equal (test A == B)
#    ne  - not equal (test A != B)
#    le  - less than or equal (test A <= B)
#    ge  - greater than or equal (test A >= B)
#    lt  - less than (test A < B)
#    gt  - greater than (test A > B)
#
#   Additionally, the eq and ne operator can have a number after it to limit
#   the test to that number of minor versions.
#
#    eq0 - equal up to the length of the shorter version
#    ne0 - not equal up to the length of the shorter version
#    eqN - equal up to N sub-version levels
#    neN - not equal up to N sub-version levels
#
#   When the condition is true, shell commands ACTION-IF-TRUE are run,
#   otherwise shell commands ACTION-IF-FALSE are run. The environment
#   variable 'ax_compare_version' is always set to either 'true' or 'false'
#   as well.
#
#   Examples:
#
#     AX_COMPARE_VERSION([3.15.7],[lt],[3.15.8])
#     AX_COMPARE_VERSION([3.15],[lt],[3.15.8])
#
#   would both be true.
#
#     AX_COMPARE_VERSION([3.15.7],[eq],[3.15.8])
#     AX_COMPARE_VERSION([3.15],[gt],[3.15.8])
#
#   would both be false.
#
#     AX_COMPARE_VERSION([3.15.7],[eq2],[3.15.8])
#
#   would be true because it is only comparing two minor versions.
#
#     AX_COMPARE_VERSION([3.15.7],[eq0],[3.15])
#
#   would be true because it is only comparing the lesser number of minor
#   versions of the two values.
#
#   Note: The characters that separate the version numbers do not matter. An
#   empty string is the same as version 0. OP is evaluated by autoconf, not
#   configure, so must be a string, not a variable.
#
#   The author would like to acknowledge Guido Draheim whose advice about
#   the m4_case and m4_ifvaln functions make this macro only include the
#   portions necessary to perform the specific comparison specified by the
#   OP argument in the final configure script.
#
# LICENSE
#
#   Copyright (c) 2008 Tim Toolan <toolan@ele.uri.edu>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 13

dnl #########################################################################
AC_DEFUN([AX_COMPARE_VERSION], [
  AC_REQUIRE([AC_PROG_AWK])

  # Used to indicate true or false condition
  ax_compare_version=false

  # Convert the two version strings to be compared into a format that
  # allows a simple string comparison.  The end result is that a version
  # string of the form 1.12.5-r617 will be converted to the form
  # 0001001200050617.  In other words, each number is zero padded to four
  # digits, and non digits are removed.
  AS_VAR_PUSHDEF([A],[ax_compare_version_A])
  A=`echo "$1" | sed -e 's/\([[0-9]]*\)/Z\1Z/g' \
                     -e 's/Z\([[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/[[^0-9]]//g'`

  AS_VAR_PUSHDEF([B],[ax_compare_version_B])
  B=`echo "$3" | sed -e 's/\([[0-9]]*\)/Z\1Z/g' \
                     -e 's/Z\([[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/[[^0-9]]//g'`

  dnl # In the case of le, ge, lt, and gt, the strings are sorted as necessary
  dnl # then the first line is used to determine if the condition is true.
  dnl # The sed right after the echo is to remove any indented white space.
  m4_case(m4_tolower($2),
  [lt],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort -r | sed "s/x${A}/false/;s/x${B}/true/;1q"`
  ],
  [gt],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort | sed "s/x${A}/false/;s/x${B}/true/;1q"`
  ],
  [le],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort | sed "s/x${A}/true/;s/x${B}/false/;1q"`
  ],
  [ge],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort -r | sed "s/x${A}/true/;s/x${B}/false/;1q"`
  ],[
    dnl Split the operator from the subversion count if present.
    m4_bmatch(m4_substr($2,2),
    [0],[
      # A count of zero means use the length of the shorter version.
      # Determine the number of characters in A and B.
      ax_compare_version_len_A=`echo "$A" | $AWK '{print(length)}'`
      ax_compare_version_len_B=`echo "$B" | $AWK '{print(length)}'`

      # Set A to no more than B's length and B to no more than A's length.
      A=`echo "$A" | sed "s/\(.\{$ax_compare_version_len_B\}\).*/\1/"`
      B=`echo "$B" | sed "s/\(.\{$ax_compare_version_len_A\}\).*/\1/"`
    ],
    [[0-9]+],[
      # A count greater than zero means use only that many subversions
      A=`echo "$A" | sed "s/\(\([[0-9]]\{4\}\)\{m4_substr($2,2)\}\).*/\1/"`
      B=`echo "$B" | sed "s/\(\([[0-9]]\{4\}\)\{m4_substr($2,2)\}\).*/\1/"`
    ],
    [.+],[
      AC_WARNING(
        [invalid OP numeric parameter: $2])
    ],[])

    # Pad zeros at end of numbers to make same length.
    ax_compare_version_tmp_A="$A`echo $B | sed 's/./0/g'`"
    B="$B`echo $A | sed 's/./0/g'`"
    A="$ax_compare_version_tmp_A"

    # Check for equality or inequality as necessary.
    m4_case(m4_tolower(m4_substr($2,0,2)),
    [eq],[
      test "x$A" = "x$B" && ax_compare_version=true
    ],
    [ne],[
      test "x$A" != "x$B" && ax_compare_version=true
    ],[
      AC_WARNING([invalid OP parameter: $2])
    ])
  ])

  AS_VAR_POPDEF([A])dnl
  AS_VAR_POPDEF([B])dnl

  dnl # Execute ACTION-IF-TRUE / ACTION-IF-FALSE.
  if test "$ax_compare_version" = "true" ; then
    m4_ifvaln([$4],[$4],[:])dnl
    m4_ifvaln([$5],[else $5])dnl
  fi
]) dnl AX_COMPARE_VERSION



dnl The following no longer appear to be part of autoconf-archive.
dnl TODO: Look into replacing them with something else
# ===========================================================================
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

dnl From pkg-config installation

dnl pkg.m4 - Macros to locate and utilise pkg-config.   -*- Autoconf -*-
dnl serial 11 (pkg-config-0.29.1)
dnl
dnl Copyright © 2004 Scott James Remnant <scott@netsplit.com>.
dnl Copyright © 2012-2015 Dan Nicholson <dbn.lists@gmail.com>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
dnl 02111-1307, USA.
dnl
dnl As a special exception to the GNU General Public License, if you
dnl distribute this file as part of a program that contains a
dnl configuration script generated by Autoconf, you may include it under
dnl the same distribution terms that you use for the rest of that
dnl program.

dnl PKG_PREREQ(MIN-VERSION)
dnl -----------------------
dnl Since: 0.29
dnl
dnl Verify that the version of the pkg-config macros are at least
dnl MIN-VERSION. Unlike PKG_PROG_PKG_CONFIG, which checks the user's
dnl installed version of pkg-config, this checks the developer's version
dnl of pkg.m4 when generating configure.
dnl
dnl To ensure that this macro is defined, also add:
dnl m4_ifndef([PKG_PREREQ],
dnl     [m4_fatal([must install pkg-config 0.29 or later before running autoconf/autogen])])
dnl
dnl See the "Since" comment for each macro you use to see what version
dnl of the macros you require.
m4_defun([PKG_PREREQ],
[m4_define([PKG_MACROS_VERSION], [0.29.1])
m4_if(m4_version_compare(PKG_MACROS_VERSION, [$1]), -1,
    [m4_fatal([pkg.m4 version $1 or higher is required but ]PKG_MACROS_VERSION[ found])])
])dnl PKG_PREREQ

dnl PKG_PROG_PKG_CONFIG([MIN-VERSION])
dnl ----------------------------------
dnl Since: 0.16
dnl
dnl Search for the pkg-config tool and set the PKG_CONFIG variable to
dnl first found in the path. Checks that the version of pkg-config found
dnl is at least MIN-VERSION. If MIN-VERSION is not specified, 0.9.0 is
dnl used since that's the first version where most current features of
dnl pkg-config existed.
AC_DEFUN([PKG_PROG_PKG_CONFIG],
[m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_(PATH|LIBDIR|SYSROOT_DIR|ALLOW_SYSTEM_(CFLAGS|LIBS)))?$])
m4_pattern_allow([^PKG_CONFIG_(DISABLE_UNINSTALLED|TOP_BUILD_DIR|DEBUG_SPEW)$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])
AC_ARG_VAR([PKG_CONFIG_PATH], [directories to add to pkg-config's search path])
AC_ARG_VAR([PKG_CONFIG_LIBDIR], [path overriding pkg-config's built-in search path])

if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test -n "$PKG_CONFIG"; then
	_pkg_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([pkg-config is at least version $_pkg_min_version])
	if $PKG_CONFIG --atleast-pkgconfig-version $_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		PKG_CONFIG=""
	fi
fi[]dnl
])dnl PKG_PROG_PKG_CONFIG

dnl PKG_CHECK_EXISTS(MODULES, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl -------------------------------------------------------------------
dnl Since: 0.18
dnl
dnl Check to see whether a particular set of modules exists. Similar to
dnl PKG_CHECK_MODULES(), but does not set variables or print errors.
dnl
dnl Please remember that m4 expands AC_REQUIRE([PKG_PROG_PKG_CONFIG])
dnl only at the first occurence in configure.ac, so if the first place
dnl it's called might be skipped (such as if it is within an "if", you
dnl have to call PKG_CHECK_EXISTS manually
AC_DEFUN([PKG_CHECK_EXISTS],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
if test -n "$PKG_CONFIG" && \
    AC_RUN_LOG([$PKG_CONFIG --exists --print-errors "$1"]); then
  m4_default([$2], [:])
m4_ifvaln([$3], [else
  $3])dnl
fi])

dnl _PKG_CONFIG([VARIABLE], [COMMAND], [MODULES])
dnl ---------------------------------------------
dnl Internal wrapper calling pkg-config via PKG_CONFIG and setting
dnl pkg_failed based on the result.
m4_define([_PKG_CONFIG],
[if test -n "$$1"; then
    pkg_cv_[]$1="$$1"
 elif test -n "$PKG_CONFIG"; then
    PKG_CHECK_EXISTS([$3],
                     [pkg_cv_[]$1=`$PKG_CONFIG --[]$2 "$3" 2>/dev/null`
		      test "x$?" != "x0" && pkg_failed=yes ],
		     [pkg_failed=yes])
 else
    pkg_failed=untried
fi[]dnl
])dnl _PKG_CONFIG

dnl _PKG_SHORT_ERRORS_SUPPORTED
dnl ---------------------------
dnl Internal check to see if pkg-config supports short errors.
AC_DEFUN([_PKG_SHORT_ERRORS_SUPPORTED],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])
if $PKG_CONFIG --atleast-pkgconfig-version 0.20; then
        _pkg_short_errors_supported=yes
else
        _pkg_short_errors_supported=no
fi[]dnl
])dnl _PKG_SHORT_ERRORS_SUPPORTED


dnl PKG_CHECK_MODULES(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
dnl   [ACTION-IF-NOT-FOUND])
dnl --------------------------------------------------------------
dnl Since: 0.4.0
dnl
dnl Note that if there is a possibility the first call to
dnl PKG_CHECK_MODULES might not happen, you should be sure to include an
dnl explicit call to PKG_PROG_PKG_CONFIG in your configure.ac
AC_DEFUN([PKG_CHECK_MODULES],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1][_CFLAGS], [C compiler flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LIBS], [linker flags for $1, overriding pkg-config])dnl

pkg_failed=no
AC_MSG_CHECKING([for $1])

_PKG_CONFIG([$1][_CFLAGS], [cflags], [$2])
_PKG_CONFIG([$1][_LIBS], [libs], [$2])

m4_define([_PKG_TEXT], [Alternatively, you may set the environment variables $1[]_CFLAGS
and $1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

if test $pkg_failed = yes; then
   	AC_MSG_RESULT([no])
        _PKG_SHORT_ERRORS_SUPPORTED
        if test $_pkg_short_errors_supported = yes; then
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --short-errors --print-errors --cflags --libs "$2" 2>&1`
        else 
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --print-errors --cflags --libs "$2" 2>&1`
        fi
	# Put the nasty error message in config.log where it belongs
	echo "$$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

	m4_default([$4], [AC_MSG_ERROR(
[Package requirements ($2) were not met:

$$1_PKG_ERRORS

Consider adjusting the PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_PKG_TEXT])[]dnl
        ])
elif test $pkg_failed = untried; then
     	AC_MSG_RESULT([no])
	m4_default([$4], [AC_MSG_FAILURE(
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the PKG_CONFIG environment variable to the full
path to pkg-config.

_PKG_TEXT

To get pkg-config, see <http://pkg-config.freedesktop.org/>.])[]dnl
        ])
else
	$1[]_CFLAGS=$pkg_cv_[]$1[]_CFLAGS
	$1[]_LIBS=$pkg_cv_[]$1[]_LIBS
        AC_MSG_RESULT([yes])
	$3
fi[]dnl
])dnl PKG_CHECK_MODULES


dnl PKG_CHECK_MODULES_STATIC(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
dnl   [ACTION-IF-NOT-FOUND])
dnl ---------------------------------------------------------------------
dnl Since: 0.29
dnl
dnl Checks for existence of MODULES and gathers its build flags with
dnl static libraries enabled. Sets VARIABLE-PREFIX_CFLAGS from --cflags
dnl and VARIABLE-PREFIX_LIBS from --libs.
dnl
dnl Note that if there is a possibility the first call to
dnl PKG_CHECK_MODULES_STATIC might not happen, you should be sure to
dnl include an explicit call to PKG_PROG_PKG_CONFIG in your
dnl configure.ac.
AC_DEFUN([PKG_CHECK_MODULES_STATIC],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
_save_PKG_CONFIG=$PKG_CONFIG
PKG_CONFIG="$PKG_CONFIG --static"
PKG_CHECK_MODULES($@)
PKG_CONFIG=$_save_PKG_CONFIG[]dnl
])dnl PKG_CHECK_MODULES_STATIC


dnl PKG_INSTALLDIR([DIRECTORY])
dnl -------------------------
dnl Since: 0.27
dnl
dnl Substitutes the variable pkgconfigdir as the location where a module
dnl should install pkg-config .pc files. By default the directory is
dnl $libdir/pkgconfig, but the default can be changed by passing
dnl DIRECTORY. The user can override through the --with-pkgconfigdir
dnl parameter.
AC_DEFUN([PKG_INSTALLDIR],
[m4_pushdef([pkg_default], [m4_default([$1], ['${libdir}/pkgconfig'])])
m4_pushdef([pkg_description],
    [pkg-config installation directory @<:@]pkg_default[@:>@])
AC_ARG_WITH([pkgconfigdir],
    [AS_HELP_STRING([--with-pkgconfigdir], pkg_description)],,
    [with_pkgconfigdir=]pkg_default)
AC_SUBST([pkgconfigdir], [$with_pkgconfigdir])
m4_popdef([pkg_default])
m4_popdef([pkg_description])
])dnl PKG_INSTALLDIR


dnl PKG_NOARCH_INSTALLDIR([DIRECTORY])
dnl --------------------------------
dnl Since: 0.27
dnl
dnl Substitutes the variable noarch_pkgconfigdir as the location where a
dnl module should install arch-independent pkg-config .pc files. By
dnl default the directory is $datadir/pkgconfig, but the default can be
dnl changed by passing DIRECTORY. The user can override through the
dnl --with-noarch-pkgconfigdir parameter.
AC_DEFUN([PKG_NOARCH_INSTALLDIR],
[m4_pushdef([pkg_default], [m4_default([$1], ['${datadir}/pkgconfig'])])
m4_pushdef([pkg_description],
    [pkg-config arch-independent installation directory @<:@]pkg_default[@:>@])
AC_ARG_WITH([noarch-pkgconfigdir],
    [AS_HELP_STRING([--with-noarch-pkgconfigdir], pkg_description)],,
    [with_noarch_pkgconfigdir=]pkg_default)
AC_SUBST([noarch_pkgconfigdir], [$with_noarch_pkgconfigdir])
m4_popdef([pkg_default])
m4_popdef([pkg_description])
])dnl PKG_NOARCH_INSTALLDIR


dnl PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
dnl [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl -------------------------------------------
dnl Since: 0.28
dnl
dnl Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])dnl PKG_CHECK_VAR

# ===========================================================================
#        https://www.gnu.org/software/autoconf-archive/ax_pthread.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
#
# DESCRIPTION
#
#   This macro figures out how to build C programs using POSIX threads. It
#   sets the PTHREAD_LIBS output variable to the threads library and linker
#   flags, and the PTHREAD_CFLAGS output variable to any special C compiler
#   flags that are needed. (The user can also force certain compiler
#   flags/libs to be tested by setting these environment variables.)
#
#   Also sets PTHREAD_CC to any special C compiler that is needed for
#   multi-threaded programs (defaults to the value of CC otherwise). (This
#   is necessary on AIX to use the special cc_r compiler alias.)
#
#   NOTE: You are assumed to not only compile your program with these flags,
#   but also to link with them as well. For example, you might link with
#   $PTHREAD_CC $CFLAGS $PTHREAD_CFLAGS $LDFLAGS ... $PTHREAD_LIBS $LIBS
#
#   If you are only building threaded programs, you may wish to use these
#   variables in your default LIBS, CFLAGS, and CC:
#
#     LIBS="$PTHREAD_LIBS $LIBS"
#     CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
#     CC="$PTHREAD_CC"
#
#   In addition, if the PTHREAD_CREATE_JOINABLE thread-attribute constant
#   has a nonstandard name, this macro defines PTHREAD_CREATE_JOINABLE to
#   that name (e.g. PTHREAD_CREATE_UNDETACHED on AIX).
#
#   Also HAVE_PTHREAD_PRIO_INHERIT is defined if pthread is found and the
#   PTHREAD_PRIO_INHERIT symbol is defined when compiling with
#   PTHREAD_CFLAGS.
#
#   ACTION-IF-FOUND is a list of shell commands to run if a threads library
#   is found, and ACTION-IF-NOT-FOUND is a list of commands to run it if it
#   is not found. If ACTION-IF-FOUND is not specified, the default action
#   will define HAVE_PTHREAD.
#
#   Please let the authors know if this macro fails on any platform, or if
#   you have any other suggestions or comments. This macro was based on work
#   by SGJ on autoconf scripts for FFTW (http://www.fftw.org/) (with help
#   from M. Frigo), as well as ac_pthread and hb_pthread macros posted by
#   Alejandro Forero Cuervo to the autoconf macro repository. We are also
#   grateful for the helpful feedback of numerous users.
#
#   Updated for Autoconf 2.68 by Daniel Richard G.
#
# LICENSE
#
#   Copyright (c) 2008 Steven G. Johnson <stevenj@alum.mit.edu>
#   Copyright (c) 2011 Daniel Richard G. <skunk@iSKUNK.ORG>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <https://www.gnu.org/licenses/>.
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

#serial 24

AU_ALIAS([ACX_PTHREAD], [AX_PTHREAD])
AC_DEFUN([AX_PTHREAD], [
AC_REQUIRE([AC_CANONICAL_HOST])
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_SED])
AC_LANG_PUSH([C])
ax_pthread_ok=no

# We used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on Tru64 or Sequent).
# It gets checked for in the link test anyway.

# First of all, check if the user has set any of the PTHREAD_LIBS,
# etcetera environment variables, and if threads linking works using
# them:
if test "x$PTHREAD_CFLAGS$PTHREAD_LIBS" != "x"; then
        ax_pthread_save_CC="$CC"
        ax_pthread_save_CFLAGS="$CFLAGS"
        ax_pthread_save_LIBS="$LIBS"
        AS_IF([test "x$PTHREAD_CC" != "x"], [CC="$PTHREAD_CC"])
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        AC_MSG_CHECKING([for pthread_join using $CC $PTHREAD_CFLAGS $PTHREAD_LIBS])
        AC_LINK_IFELSE([AC_LANG_CALL([], [pthread_join])], [ax_pthread_ok=yes])
        AC_MSG_RESULT([$ax_pthread_ok])
        if test "x$ax_pthread_ok" = "xno"; then
                PTHREAD_LIBS=""
                PTHREAD_CFLAGS=""
        fi
        CC="$ax_pthread_save_CC"
        CFLAGS="$ax_pthread_save_CFLAGS"
        LIBS="$ax_pthread_save_LIBS"
fi

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all, and "pthread-config"
# which is a program returning the flags for the Pth emulation library.

ax_pthread_flags="pthreads none -Kthread -pthread -pthreads -mthreads pthread --thread-safe -mt pthread-config"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads), Tru64
#           (Note: HP C rejects this with "bad form for `-t' option")
# -pthreads: Solaris/gcc (Note: HP C also rejects)
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads and
#      -D_REENTRANT too), HP C (must be checked before -lpthread, which
#      is present but should not be used directly; and before -mthreads,
#      because the compiler interprets this as "-mt" + "-hreads")
# -mthreads: Mingw32/gcc, Lynx/gcc
# pthread: Linux, etcetera
# --thread-safe: KAI C++
# pthread-config: use pthread-config program (for GNU Pth library)

case $host_os in

        freebsd*)

        # -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
        # lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)

        ax_pthread_flags="-kthread lthread $ax_pthread_flags"
        ;;

        hpux*)

        # From the cc(1) man page: "[-mt] Sets various -D flags to enable
        # multi-threading and also sets -lpthread."

        ax_pthread_flags="-mt -pthread pthread $ax_pthread_flags"
        ;;

        openedition*)

        # IBM z/OS requires a feature-test macro to be defined in order to
        # enable POSIX threads at all, so give the user a hint if this is
        # not set. (We don't define these ourselves, as they can affect
        # other portions of the system API in unpredictable ways.)

        AC_EGREP_CPP([AX_PTHREAD_ZOS_MISSING],
            [
#            if !defined(_OPEN_THREADS) && !defined(_UNIX03_THREADS)
             AX_PTHREAD_ZOS_MISSING
#            endif
            ],
            [AC_MSG_WARN([IBM z/OS requires -D_OPEN_THREADS or -D_UNIX03_THREADS to enable pthreads support.])])
        ;;

        solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed. (N.B.: The stubs are missing
        # pthread_cleanup_push, or rather a function called by this macro,
        # so we could check for that, but who knows whether they'll stub
        # that too in a future libc.)  So we'll check first for the
        # standard Solaris way of linking pthreads (-mt -lpthread).

        ax_pthread_flags="-mt,pthread pthread $ax_pthread_flags"
        ;;
esac

# GCC generally uses -pthread, or -pthreads on some platforms (e.g. SPARC)

AS_IF([test "x$GCC" = "xyes"],
      [ax_pthread_flags="-pthread -pthreads $ax_pthread_flags"])

# The presence of a feature test macro requesting re-entrant function
# definitions is, on some systems, a strong hint that pthreads support is
# correctly enabled

case $host_os in
        darwin* | hpux* | linux* | osf* | solaris*)
        ax_pthread_check_macro="_REENTRANT"
        ;;

        aix*)
        ax_pthread_check_macro="_THREAD_SAFE"
        ;;

        *)
        ax_pthread_check_macro="--"
        ;;
esac
AS_IF([test "x$ax_pthread_check_macro" = "x--"],
      [ax_pthread_check_cond=0],
      [ax_pthread_check_cond="!defined($ax_pthread_check_macro)"])

# Are we compiling with Clang?

AC_CACHE_CHECK([whether $CC is Clang],
    [ax_cv_PTHREAD_CLANG],
    [ax_cv_PTHREAD_CLANG=no
     # Note that Autoconf sets GCC=yes for Clang as well as GCC
     if test "x$GCC" = "xyes"; then
        AC_EGREP_CPP([AX_PTHREAD_CC_IS_CLANG],
            [/* Note: Clang 2.7 lacks __clang_[a-z]+__ */
#            if defined(__clang__) && defined(__llvm__)
             AX_PTHREAD_CC_IS_CLANG
#            endif
            ],
            [ax_cv_PTHREAD_CLANG=yes])
     fi
    ])
ax_pthread_clang="$ax_cv_PTHREAD_CLANG"

ax_pthread_clang_warning=no

# Clang needs special handling, because older versions handle the -pthread
# option in a rather... idiosyncratic way

if test "x$ax_pthread_clang" = "xyes"; then

        # Clang takes -pthread; it has never supported any other flag

        # (Note 1: This will need to be revisited if a system that Clang
        # supports has POSIX threads in a separate library.  This tends not
        # to be the way of modern systems, but it's conceivable.)

        # (Note 2: On some systems, notably Darwin, -pthread is not needed
        # to get POSIX threads support; the API is always present and
        # active.  We could reasonably leave PTHREAD_CFLAGS empty.  But
        # -pthread does define _REENTRANT, and while the Darwin headers
        # ignore this macro, third-party headers might not.)

        PTHREAD_CFLAGS="-pthread"
        PTHREAD_LIBS=

        ax_pthread_ok=yes

        # However, older versions of Clang make a point of warning the user
        # that, in an invocation where only linking and no compilation is
        # taking place, the -pthread option has no effect ("argument unused
        # during compilation").  They expect -pthread to be passed in only
        # when source code is being compiled.
        #
        # Problem is, this is at odds with the way Automake and most other
        # C build frameworks function, which is that the same flags used in
        # compilation (CFLAGS) are also used in linking.  Many systems
        # supported by AX_PTHREAD require exactly this for POSIX threads
        # support, and in fact it is often not straightforward to specify a
        # flag that is used only in the compilation phase and not in
        # linking.  Such a scenario is extremely rare in practice.
        #
        # Even though use of the -pthread flag in linking would only print
        # a warning, this can be a nuisance for well-run software projects
        # that build with -Werror.  So if the active version of Clang has
        # this misfeature, we search for an option to squash it.

        AC_CACHE_CHECK([whether Clang needs flag to prevent "argument unused" warning when linking with -pthread],
            [ax_cv_PTHREAD_CLANG_NO_WARN_FLAG],
            [ax_cv_PTHREAD_CLANG_NO_WARN_FLAG=unknown
             # Create an alternate version of $ac_link that compiles and
             # links in two steps (.c -> .o, .o -> exe) instead of one
             # (.c -> exe), because the warning occurs only in the second
             # step
             ax_pthread_save_ac_link="$ac_link"
             ax_pthread_sed='s/conftest\.\$ac_ext/conftest.$ac_objext/g'
             ax_pthread_link_step=`$as_echo "$ac_link" | sed "$ax_pthread_sed"`
             ax_pthread_2step_ac_link="($ac_compile) && (echo ==== >&5) && ($ax_pthread_link_step)"
             ax_pthread_save_CFLAGS="$CFLAGS"
             for ax_pthread_try in '' -Qunused-arguments -Wno-unused-command-line-argument unknown; do
                AS_IF([test "x$ax_pthread_try" = "xunknown"], [break])
                CFLAGS="-Werror -Wunknown-warning-option $ax_pthread_try -pthread $ax_pthread_save_CFLAGS"
                ac_link="$ax_pthread_save_ac_link"
                AC_LINK_IFELSE([AC_LANG_SOURCE([[int main(void){return 0;}]])],
                    [ac_link="$ax_pthread_2step_ac_link"
                     AC_LINK_IFELSE([AC_LANG_SOURCE([[int main(void){return 0;}]])],
                         [break])
                    ])
             done
             ac_link="$ax_pthread_save_ac_link"
             CFLAGS="$ax_pthread_save_CFLAGS"
             AS_IF([test "x$ax_pthread_try" = "x"], [ax_pthread_try=no])
             ax_cv_PTHREAD_CLANG_NO_WARN_FLAG="$ax_pthread_try"
            ])

        case "$ax_cv_PTHREAD_CLANG_NO_WARN_FLAG" in
                no | unknown) ;;
                *) PTHREAD_CFLAGS="$ax_cv_PTHREAD_CLANG_NO_WARN_FLAG $PTHREAD_CFLAGS" ;;
        esac

fi # $ax_pthread_clang = yes

if test "x$ax_pthread_ok" = "xno"; then
for ax_pthread_try_flag in $ax_pthread_flags; do

        case $ax_pthread_try_flag in
                none)
                AC_MSG_CHECKING([whether pthreads work without any flags])
                ;;

                -mt,pthread)
                AC_MSG_CHECKING([whether pthreads work with -mt -lpthread])
                PTHREAD_CFLAGS="-mt"
                PTHREAD_LIBS="-lpthread"
                ;;

                -*)
                AC_MSG_CHECKING([whether pthreads work with $ax_pthread_try_flag])
                PTHREAD_CFLAGS="$ax_pthread_try_flag"
                ;;

                pthread-config)
                AC_CHECK_PROG([ax_pthread_config], [pthread-config], [yes], [no])
                AS_IF([test "x$ax_pthread_config" = "xno"], [continue])
                PTHREAD_CFLAGS="`pthread-config --cflags`"
                PTHREAD_LIBS="`pthread-config --ldflags` `pthread-config --libs`"
                ;;

                *)
                AC_MSG_CHECKING([for the pthreads library -l$ax_pthread_try_flag])
                PTHREAD_LIBS="-l$ax_pthread_try_flag"
                ;;
        esac

        ax_pthread_save_CFLAGS="$CFLAGS"
        ax_pthread_save_LIBS="$LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.

        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>
#                       if $ax_pthread_check_cond
#                        error "$ax_pthread_check_macro must be defined"
#                       endif
                        static void routine(void *a) { a = 0; }
                        static void *start_routine(void *a) { return a; }],
                       [pthread_t th; pthread_attr_t attr;
                        pthread_create(&th, 0, start_routine, 0);
                        pthread_join(th, 0);
                        pthread_attr_init(&attr);
                        pthread_cleanup_push(routine, 0);
                        pthread_cleanup_pop(0) /* ; */])],
            [ax_pthread_ok=yes],
            [])

        CFLAGS="$ax_pthread_save_CFLAGS"
        LIBS="$ax_pthread_save_LIBS"

        AC_MSG_RESULT([$ax_pthread_ok])
        AS_IF([test "x$ax_pthread_ok" = "xyes"], [break])

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi

# Various other checks:
if test "x$ax_pthread_ok" = "xyes"; then
        ax_pthread_save_CFLAGS="$CFLAGS"
        ax_pthread_save_LIBS="$LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"

        # Detect AIX lossage: JOINABLE attribute is called UNDETACHED.
        AC_CACHE_CHECK([for joinable pthread attribute],
            [ax_cv_PTHREAD_JOINABLE_ATTR],
            [ax_cv_PTHREAD_JOINABLE_ATTR=unknown
             for ax_pthread_attr in PTHREAD_CREATE_JOINABLE PTHREAD_CREATE_UNDETACHED; do
                 AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>],
                                                 [int attr = $ax_pthread_attr; return attr /* ; */])],
                                [ax_cv_PTHREAD_JOINABLE_ATTR=$ax_pthread_attr; break],
                                [])
             done
            ])
        AS_IF([test "x$ax_cv_PTHREAD_JOINABLE_ATTR" != "xunknown" && \
               test "x$ax_cv_PTHREAD_JOINABLE_ATTR" != "xPTHREAD_CREATE_JOINABLE" && \
               test "x$ax_pthread_joinable_attr_defined" != "xyes"],
              [AC_DEFINE_UNQUOTED([PTHREAD_CREATE_JOINABLE],
                                  [$ax_cv_PTHREAD_JOINABLE_ATTR],
                                  [Define to necessary symbol if this constant
                                   uses a non-standard name on your system.])
               ax_pthread_joinable_attr_defined=yes
              ])

        AC_CACHE_CHECK([whether more special flags are required for pthreads],
            [ax_cv_PTHREAD_SPECIAL_FLAGS],
            [ax_cv_PTHREAD_SPECIAL_FLAGS=no
             case $host_os in
             solaris*)
             ax_cv_PTHREAD_SPECIAL_FLAGS="-D_POSIX_PTHREAD_SEMANTICS"
             ;;
             esac
            ])
        AS_IF([test "x$ax_cv_PTHREAD_SPECIAL_FLAGS" != "xno" && \
               test "x$ax_pthread_special_flags_added" != "xyes"],
              [PTHREAD_CFLAGS="$ax_cv_PTHREAD_SPECIAL_FLAGS $PTHREAD_CFLAGS"
               ax_pthread_special_flags_added=yes])

        AC_CACHE_CHECK([for PTHREAD_PRIO_INHERIT],
            [ax_cv_PTHREAD_PRIO_INHERIT],
            [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
                                             [[int i = PTHREAD_PRIO_INHERIT;]])],
                            [ax_cv_PTHREAD_PRIO_INHERIT=yes],
                            [ax_cv_PTHREAD_PRIO_INHERIT=no])
            ])
        AS_IF([test "x$ax_cv_PTHREAD_PRIO_INHERIT" = "xyes" && \
               test "x$ax_pthread_prio_inherit_defined" != "xyes"],
              [AC_DEFINE([HAVE_PTHREAD_PRIO_INHERIT], [1], [Have PTHREAD_PRIO_INHERIT.])
               ax_pthread_prio_inherit_defined=yes
              ])

        CFLAGS="$ax_pthread_save_CFLAGS"
        LIBS="$ax_pthread_save_LIBS"

        # More AIX lossage: compile with *_r variant
        if test "x$GCC" != "xyes"; then
            case $host_os in
                aix*)
                AS_CASE(["x/$CC"],
                    [x*/c89|x*/c89_128|x*/c99|x*/c99_128|x*/cc|x*/cc128|x*/xlc|x*/xlc_v6|x*/xlc128|x*/xlc128_v6],
                    [#handle absolute path differently from PATH based program lookup
                     AS_CASE(["x$CC"],
                         [x/*],
                         [AS_IF([AS_EXECUTABLE_P([${CC}_r])],[PTHREAD_CC="${CC}_r"])],
                         [AC_CHECK_PROGS([PTHREAD_CC],[${CC}_r],[$CC])])])
                ;;
            esac
        fi
fi

test -n "$PTHREAD_CC" || PTHREAD_CC="$CC"

AC_SUBST([PTHREAD_LIBS])
AC_SUBST([PTHREAD_CFLAGS])
AC_SUBST([PTHREAD_CC])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "x$ax_pthread_ok" = "xyes"; then
        ifelse([$1],,[AC_DEFINE([HAVE_PTHREAD],[1],[Define if you have POSIX threads libraries and header files.])],[$1])
        :
else
        ax_pthread_ok=no
        $2
fi
AC_LANG_POP
])dnl AX_PTHREAD

# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_path_generic.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PATH_GENERIC(LIBRARY,[MINIMUM-VERSION,[SED-EXPR-EXTRACTOR]],[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND],[CONFIG-SCRIPTS],[CFLAGS-ARG],[LIBS-ARG])
#
# DESCRIPTION
#
#   Runs the LIBRARY-config script and defines LIBRARY_CFLAGS and
#   LIBRARY_LIBS unless the user had predefined them in the environment.
#
#   The script must support `--cflags' and `--libs' args. If MINIMUM-VERSION
#   is specified, the script must also support the `--version' arg. If the
#   `--with-library-[exec-]prefix' arguments to ./configure are given, it
#   must also support `--prefix' and `--exec-prefix'. Preferably use
#   CONFIG-SCRIPTS as config script, CFLAGS-ARG instead of `--cflags` and
#   LIBS-ARG instead of `--libs`, if given.
#
#   The SED-EXPR-EXTRACTOR parameter represents the expression used in sed
#   to extract the version number. Use it if your 'foo-config --version'
#   dumps something like 'Foo library v1.0.0 (alfa)' instead of '1.0.0'.
#
#   The macro respects LIBRARY_CONFIG, LIBRARY_CFLAGS and LIBRARY_LIBS
#   variables. If the first one is defined, it specifies the name of the
#   config script to use. If the latter two are defined, the script is not
#   ran at all and their values are used instead (if only one of them is
#   defined, the empty value of the remaining one is still used).
#
#   Example:
#
#     AX_PATH_GENERIC(Foo, 1.0.0)
#
#   would run `foo-config --version' and check that it is at least 1.0.0, if
#   successful the following variables would be defined and substituted:
#
#     FOO_CFLAGS to `foo-config --cflags`
#     FOO_LIBS   to `foo-config --libs`
#
#   Example:
#
#     AX_PATH_GENERIC([Bar],,,[
#        AC_MSG_ERROR([Cannot find Bar library])
#     ])
#
#   would check for bar-config program, defining and substituting the
#   following variables:
#
#     BAR_CFLAGS to `bar-config --cflags`
#     BAR_LIBS   to `bar-config --libs`
#
#   Example:
#
#     ./configure BAZ_LIBS=/usr/lib/libbaz.a
#
#   would link with a static version of baz library even if `baz-config
#   --libs` returns just "-lbaz" that would normally result in using the
#   shared library.
#
#   This macro is a rearranged version of AC_PATH_GENERIC from Angus Lees.
#
# LICENSE
#
#   Copyright (c) 2009 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 17

AU_ALIAS([AC_PATH_GENERIC], [AX_PATH_GENERIC])
AC_DEFUN([AX_PATH_GENERIC],[
  AC_REQUIRE([AC_PROG_SED])

  dnl we're going to need uppercase and lowercase versions of the
  dnl string `LIBRARY'
  pushdef([UP],   translit([$1], [a-z], [A-Z]))dnl
  pushdef([DOWN], translit([$1], [A-Z], [a-z]))dnl

  AC_ARG_WITH(DOWN-prefix,[AS_HELP_STRING([--with-]DOWN[-prefix=PREFIX], [Prefix where $1 is installed (optional)])],
    DOWN[]_config_prefix="$withval", DOWN[]_config_prefix="")
  AC_ARG_WITH(DOWN-exec-prefix,[AS_HELP_STRING([--with-]DOWN[-exec-prefix=EPREFIX], [Exec prefix where $1 is installed (optional)])],
    DOWN[]_config_exec_prefix="$withval", DOWN[]_config_exec_prefix="")

  AC_ARG_VAR(UP[]_CONFIG, [config script used for $1])
  AC_ARG_VAR(UP[]_CFLAGS, [CFLAGS used for $1])
  AC_ARG_VAR(UP[]_LIBS,   [LIBS used for $1])

  AS_IF([test x"$UP[]_CFLAGS" != x -o x"$UP[]_LIBS" != x],[
    dnl Don't run config script at all, use user-provided values instead.
    AC_SUBST(UP[]_CFLAGS)
    AC_SUBST(UP[]_LIBS)
    :
    $4
  ],[
    AS_IF([test x$DOWN[]_config_exec_prefix != x],[
      DOWN[]_config_args="$DOWN[]_config_args --exec-prefix=$DOWN[]_config_exec_prefix"
      AS_IF([test x${UP[]_CONFIG+set} != xset],[
	UP[]_CONFIG=$DOWN[]_config_exec_prefix/bin/DOWN-config
      ])
    ])
    AS_IF([test x$DOWN[]_config_prefix != x],[
      DOWN[]_config_args="$DOWN[]_config_args --prefix=$DOWN[]_config_prefix"
      AS_IF([test x${UP[]_CONFIG+set} != xset],[
	UP[]_CONFIG=$DOWN[]_config_prefix/bin/DOWN-config
      ])
    ])

    AC_PATH_PROGS(UP[]_CONFIG,[$6 DOWN-config],[no])
    AS_IF([test "$UP[]_CONFIG" = "no"],[
      :
      $5
    ],[
      dnl Get the CFLAGS from LIBRARY-config script
      AS_IF([test x"$7" = x],[
	UP[]_CFLAGS="`$UP[]_CONFIG $DOWN[]_config_args --cflags`"
      ],[
	UP[]_CFLAGS="`$UP[]_CONFIG $DOWN[]_config_args $7`"
      ])

      dnl Get the LIBS from LIBRARY-config script
      AS_IF([test x"$8" = x],[
	UP[]_LIBS="`$UP[]_CONFIG $DOWN[]_config_args --libs`"
      ],[
	UP[]_LIBS="`$UP[]_CONFIG $DOWN[]_config_args $8`"
      ])

      AS_IF([test x"$2" != x],[
	dnl Check for provided library version
	AS_IF([test x"$3" != x],[
	  dnl Use provided sed expression
	  DOWN[]_version="`$UP[]_CONFIG $DOWN[]_config_args --version | $SED -e $3`"
	],[
	  DOWN[]_version="`$UP[]_CONFIG $DOWN[]_config_args --version | $SED -e 's/^\ *\(.*\)\ *$/\1/'`"
	])

	AC_MSG_CHECKING([for $1 ($DOWN[]_version) >= $2])
	AX_COMPARE_VERSION($DOWN[]_version,[ge],[$2],[
	  AC_MSG_RESULT([yes])

	  AC_SUBST(UP[]_CFLAGS)
	  AC_SUBST(UP[]_LIBS)
	  :
	  $4
	],[
	  AC_MSG_RESULT([no])
	  :
	  $5
	])
      ],[
	AC_SUBST(UP[]_CFLAGS)
	AC_SUBST(UP[]_LIBS)
	:
	$4
      ])
    ])
  ])

  popdef([UP])
  popdef([DOWN])
])

# ===========================================================================
#       https://www.gnu.org/software/autoconf-archive/ax_check_icu.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_ICU(version, action-if, action-if-not)
#
# DESCRIPTION
#
#   Defines ICU_LIBS, ICU_CFLAGS, ICU_CXXFLAGS. See icu-config(1) man page.
#
# LICENSE
#
#   Copyright (c) 2008 Akos Maroy <darkeye@tyrell.hu>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 7

AU_ALIAS([AC_CHECK_ICU], [AX_CHECK_ICU])
AC_DEFUN([AX_CHECK_ICU], [
  succeeded=no

  if test -z "$ICU_CONFIG"; then
    AC_PATH_PROG(ICU_CONFIG, icu-config, no)
  fi

  if test "$ICU_CONFIG" = "no" ; then
    echo "*** The icu-config script could not be found. Make sure it is"
    echo "*** in your path, and that taglib is properly installed."
    echo "*** Or see http://ibm.com/software/globalization/icu/"
  else
    ICU_VERSION=`$ICU_CONFIG --version`
    AC_MSG_CHECKING(for ICU >= $1)
        VERSION_CHECK=`expr $ICU_VERSION \>\= $1`
        if test "$VERSION_CHECK" = "1" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING(ICU_CPPFLAGS)
            ICU_CPPFLAGS=`$ICU_CONFIG --cppflags`
            AC_MSG_RESULT($ICU_CPPFLAGS)

            AC_MSG_CHECKING(ICU_CFLAGS)
            ICU_CFLAGS=`$ICU_CONFIG --cflags`
            AC_MSG_RESULT($ICU_CFLAGS)

            AC_MSG_CHECKING(ICU_CXXFLAGS)
            ICU_CXXFLAGS=`$ICU_CONFIG --cxxflags`
            AC_MSG_RESULT($ICU_CXXFLAGS)

            AC_MSG_CHECKING(ICU_LIBS)
            ICU_LIBS=`$ICU_CONFIG --ldflags`
            AC_MSG_RESULT($ICU_LIBS)
        else
            ICU_CPPFLAGS=""
            ICU_CFLAGS=""
            ICU_CXXFLAGS=""
            ICU_LIBS=""
            ## If we have a custom action on failure, don't print errors, but
            ## do set a variable so people can do so.
            ifelse([$3], ,echo "can't find ICU >= $1",)
        fi

        AC_SUBST(ICU_CPPFLAGS)
        AC_SUBST(ICU_CFLAGS)
        AC_SUBST(ICU_CXXFLAGS)
        AC_SUBST(ICU_LIBS)
  fi

  if test $succeeded = yes; then
     ifelse([$2], , :, [$2])
  else
     ifelse([$3], , AC_MSG_ERROR([Library requirements (ICU) not met.]), [$3])
  fi
])

# ===========================================================================
#       https://www.gnu.org/software/autoconf-archive/ax_lib_curl.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_CURL([VERSION],[ACTION-IF-SUCCESS],[ACTION-IF-FAILURE])
#
# DESCRIPTION
#
#   Checks for minimum curl library version VERSION. If successful executes
#   ACTION-IF-SUCCESS otherwise ACTION-IF-FAILURE.
#
#   Defines CURL_LIBS and CURL_CFLAGS.
#
#   A simple example:
#
#     AX_LIB_CURL([7.19.4],,[
#       AC_MSG_ERROR([Your system lacks libcurl >= 7.19.4])
#     ])
#
#   This macro is a rearranged version of AC_LIB_CURL from Akos Maroy.
#
# LICENSE
#
#   Copyright (c) 2009 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 9

AU_ALIAS([AC_CHECK_CURL], [AX_LIB_CURL])
AC_DEFUN([AX_LIB_CURL], [
  AX_PATH_GENERIC([curl],[$1],'s/^libcurl\ \+//',[$2],[$3])
])

# ===========================================================================
#      https://www.gnu.org/software/autoconf-archive/ax_check_zlib.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_ZLIB([action-if-found], [action-if-not-found])
#
# DESCRIPTION
#
#   This macro searches for an installed zlib library. If nothing was
#   specified when calling configure, it searches first in /usr/local and
#   then in /usr, /opt/local and /sw. If the --with-zlib=DIR is specified,
#   it will try to find it in DIR/include/zlib.h and DIR/lib/libz.a. If
#   --without-zlib is specified, the library is not searched at all.
#
#   If either the header file (zlib.h) or the library (libz) is not found,
#   shell commands 'action-if-not-found' is run. If 'action-if-not-found' is
#   not specified, the configuration exits on error, asking for a valid zlib
#   installation directory or --without-zlib.
#
#   If both header file and library are found, shell commands
#   'action-if-found' is run. If 'action-if-found' is not specified, the
#   default action appends '-I${ZLIB_HOME}/include' to CPFLAGS, appends
#   '-L$ZLIB_HOME}/lib' to LDFLAGS, prepends '-lz' to LIBS, and calls
#   AC_DEFINE(HAVE_LIBZ). You should use autoheader to include a definition
#   for this symbol in a config.h file. Sample usage in a C/C++ source is as
#   follows:
#
#     #ifdef HAVE_LIBZ
#     #include <zlib.h>
#     #endif /* HAVE_LIBZ */
#
# LICENSE
#
#   Copyright (c) 2008 Loic Dachary <loic@senga.org>
#   Copyright (c) 2010 Bastien Chevreux <bach@chevreux.org>
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
#   with this program. If not, see <https://www.gnu.org/licenses/>.
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

#serial 16

AU_ALIAS([CHECK_ZLIB], [AX_CHECK_ZLIB])
AC_DEFUN([AX_CHECK_ZLIB],
#
# Handle user hints
#
[AC_MSG_CHECKING(if zlib is wanted)
zlib_places="/usr/local /usr /opt/local /sw"
AC_ARG_WITH([zlib],
[  --with-zlib=DIR         root directory path of zlib installation @<:@defaults to
                          /usr/local or /usr if not found in /usr/local@:>@
  --without-zlib          to disable zlib usage completely],
[if test "$withval" != no ; then
  AC_MSG_RESULT(yes)
  if test -d "$withval"
  then
    zlib_places="$withval $zlib_places"
  else
    AC_MSG_WARN([Sorry, $withval does not exist, checking usual places])
  fi
else
  zlib_places=
  AC_MSG_RESULT(no)
fi],
[AC_MSG_RESULT(yes)])

#
# Locate zlib, if wanted
#
if test -n "${zlib_places}"
then
	# check the user supplied or any other more or less 'standard' place:
	#   Most UNIX systems      : /usr/local and /usr
	#   MacPorts / Fink on OSX : /opt/local respectively /sw
	for ZLIB_HOME in ${zlib_places} ; do
	  if test -f "${ZLIB_HOME}/include/zlib.h"; then break; fi
	  ZLIB_HOME=""
	done

  ZLIB_OLD_LDFLAGS=$LDFLAGS
  ZLIB_OLD_CPPFLAGS=$CPPFLAGS
  if test -n "${ZLIB_HOME}"; then
        LDFLAGS="$LDFLAGS -L${ZLIB_HOME}/lib"
        CPPFLAGS="$CPPFLAGS -I${ZLIB_HOME}/include"
  fi
  AC_LANG_PUSH([C])
  AC_CHECK_LIB([z], [inflateEnd], [zlib_cv_libz=yes], [zlib_cv_libz=no])
  AC_CHECK_HEADER([zlib.h], [zlib_cv_zlib_h=yes], [zlib_cv_zlib_h=no])
  AC_LANG_POP([C])
  if test "$zlib_cv_libz" = "yes" && test "$zlib_cv_zlib_h" = "yes"
  then
    #
    # If both library and header were found, action-if-found
    #
    m4_ifblank([$1],[
                CPPFLAGS="$CPPFLAGS -I${ZLIB_HOME}/include"
                LDFLAGS="$LDFLAGS -L${ZLIB_HOME}/lib"
                LIBS="-lz $LIBS"
                AC_DEFINE([HAVE_LIBZ], [1],
                          [Define to 1 if you have `z' library (-lz)])
               ],[
                # Restore variables
                LDFLAGS="$ZLIB_OLD_LDFLAGS"
                CPPFLAGS="$ZLIB_OLD_CPPFLAGS"
                $1
               ])
  else
    #
    # If either header or library was not found, action-if-not-found
    #
    m4_default([$2],[
                AC_MSG_ERROR([either specify a valid zlib installation with --with-zlib=DIR or disable zlib usage with --without-zlib])
                ])
  fi
fi
])

# ===========================================================================
#      https://www.gnu.org/software/autoconf-archive/ax_count_cpus.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_COUNT_CPUS([ACTION-IF-DETECTED],[ACTION-IF-NOT-DETECTED])
#
# DESCRIPTION
#
#   Attempt to count the number of logical processor cores (including
#   virtual and HT cores) currently available to use on the machine and
#   place detected value in CPU_COUNT variable.
#
#   On successful detection, ACTION-IF-DETECTED is executed if present. If
#   the detection fails, then ACTION-IF-NOT-DETECTED is triggered. The
#   default ACTION-IF-NOT-DETECTED is to set CPU_COUNT to 1.
#
# LICENSE
#
#   Copyright (c) 2014,2016 Karlson2k (Evgeny Grin) <k2k@narod.ru>
#   Copyright (c) 2012 Brian Aker <brian@tangent.org>
#   Copyright (c) 2008 Michael Paul Bailey <jinxidoru@byu.net>
#   Copyright (c) 2008 Christophe Tournayre <turn3r@users.sourceforge.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 22

  AC_DEFUN([AX_COUNT_CPUS],[dnl
      AC_REQUIRE([AC_CANONICAL_HOST])dnl
      AC_REQUIRE([AC_PROG_EGREP])dnl
      AC_MSG_CHECKING([the number of available CPUs])
      CPU_COUNT="0"

      # Try generic methods

      # 'getconf' is POSIX utility, but '_NPROCESSORS_ONLN' and
      # 'NPROCESSORS_ONLN' are platform-specific
      command -v getconf >/dev/null 2>&1 && \
        CPU_COUNT=`getconf _NPROCESSORS_ONLN 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null` || CPU_COUNT="0"
      AS_IF([[test "$CPU_COUNT" -gt "0" 2>/dev/null || ! command -v nproc >/dev/null 2>&1]],[[: # empty]],[dnl
        # 'nproc' is part of GNU Coreutils and is widely available
        CPU_COUNT=`OMP_NUM_THREADS='' nproc 2>/dev/null` || CPU_COUNT=`nproc 2>/dev/null` || CPU_COUNT="0"
      ])dnl

      AS_IF([[test "$CPU_COUNT" -gt "0" 2>/dev/null]],[[: # empty]],[dnl
        # Try platform-specific preferred methods
        AS_CASE([[$host_os]],dnl
          [[*linux*]],[[CPU_COUNT=`lscpu -p 2>/dev/null | $EGREP -e '^@<:@0-9@:>@+,' -c` || CPU_COUNT="0"]],dnl
          [[*darwin*]],[[CPU_COUNT=`sysctl -n hw.logicalcpu 2>/dev/null` || CPU_COUNT="0"]],dnl
          [[freebsd*]],[[command -v sysctl >/dev/null 2>&1 && CPU_COUNT=`sysctl -n kern.smp.cpus 2>/dev/null` || CPU_COUNT="0"]],dnl
          [[netbsd*]], [[command -v sysctl >/dev/null 2>&1 && CPU_COUNT=`sysctl -n hw.ncpuonline 2>/dev/null` || CPU_COUNT="0"]],dnl
          [[solaris*]],[[command -v psrinfo >/dev/null 2>&1 && CPU_COUNT=`psrinfo 2>/dev/null | $EGREP -e '^@<:@0-9@:>@.*on-line' -c 2>/dev/null` || CPU_COUNT="0"]],dnl
          [[mingw*]],[[CPU_COUNT=`ls -qpU1 /proc/registry/HKEY_LOCAL_MACHINE/HARDWARE/DESCRIPTION/System/CentralProcessor/ 2>/dev/null | $EGREP -e '^@<:@0-9@:>@+/' -c` || CPU_COUNT="0"]],dnl
          [[msys*]],[[CPU_COUNT=`ls -qpU1 /proc/registry/HKEY_LOCAL_MACHINE/HARDWARE/DESCRIPTION/System/CentralProcessor/ 2>/dev/null | $EGREP -e '^@<:@0-9@:>@+/' -c` || CPU_COUNT="0"]],dnl
          [[cygwin*]],[[CPU_COUNT=`ls -qpU1 /proc/registry/HKEY_LOCAL_MACHINE/HARDWARE/DESCRIPTION/System/CentralProcessor/ 2>/dev/null | $EGREP -e '^@<:@0-9@:>@+/' -c` || CPU_COUNT="0"]]dnl
        )dnl
      ])dnl

      AS_IF([[test "$CPU_COUNT" -gt "0" 2>/dev/null || ! command -v sysctl >/dev/null 2>&1]],[[: # empty]],[dnl
        # Try less preferred generic method
        # 'hw.ncpu' exist on many platforms, but not on GNU/Linux
        CPU_COUNT=`sysctl -n hw.ncpu 2>/dev/null` || CPU_COUNT="0"
      ])dnl

      AS_IF([[test "$CPU_COUNT" -gt "0" 2>/dev/null]],[[: # empty]],[dnl
      # Try platform-specific fallback methods
      # They can be less accurate and slower then preferred methods
        AS_CASE([[$host_os]],dnl
          [[*linux*]],[[CPU_COUNT=`$EGREP -e '^processor' -c /proc/cpuinfo 2>/dev/null` || CPU_COUNT="0"]],dnl
          [[*darwin*]],[[CPU_COUNT=`system_profiler SPHardwareDataType 2>/dev/null | $EGREP -i -e 'number of cores:'|cut -d : -f 2 -s|tr -d ' '` || CPU_COUNT="0"]],dnl
          [[freebsd*]],[[CPU_COUNT=`dmesg 2>/dev/null| $EGREP -e '^cpu@<:@0-9@:>@+: '|sort -u|$EGREP -e '^' -c` || CPU_COUNT="0"]],dnl
          [[netbsd*]], [[CPU_COUNT=`command -v cpuctl >/dev/null 2>&1 && cpuctl list 2>/dev/null| $EGREP -e '^@<:@0-9@:>@+ .* online ' -c` || \
                           CPU_COUNT=`dmesg 2>/dev/null| $EGREP -e '^cpu@<:@0-9@:>@+ at'|sort -u|$EGREP -e '^' -c` || CPU_COUNT="0"]],dnl
          [[solaris*]],[[command -v kstat >/dev/null 2>&1 && CPU_COUNT=`kstat -m cpu_info -s state -p 2>/dev/null | $EGREP -c -e 'on-line'` || \
                           CPU_COUNT=`kstat -m cpu_info 2>/dev/null | $EGREP -c -e 'module: cpu_info'` || CPU_COUNT="0"]],dnl
          [[mingw*]],[AS_IF([[CPU_COUNT=`reg query 'HKLM\\Hardware\\Description\\System\\CentralProcessor' 2>/dev/null | $EGREP -e '\\\\@<:@0-9@:>@+$' -c`]],dnl
                        [[: # empty]],[[test "$NUMBER_OF_PROCESSORS" -gt "0" 2>/dev/null && CPU_COUNT="$NUMBER_OF_PROCESSORS"]])],dnl
          [[msys*]],[[test "$NUMBER_OF_PROCESSORS" -gt "0" 2>/dev/null && CPU_COUNT="$NUMBER_OF_PROCESSORS"]],dnl
          [[cygwin*]],[[test "$NUMBER_OF_PROCESSORS" -gt "0" 2>/dev/null && CPU_COUNT="$NUMBER_OF_PROCESSORS"]]dnl
        )dnl
      ])dnl

      AS_IF([[test "x$CPU_COUNT" != "x0" && test "$CPU_COUNT" -gt 0 2>/dev/null]],[dnl
          AC_MSG_RESULT([[$CPU_COUNT]])
          m4_ifvaln([$1],[$1],)dnl
        ],[dnl
          m4_ifval([$2],[dnl
            AS_UNSET([[CPU_COUNT]])
            AC_MSG_RESULT([[unable to detect]])
            $2
          ], [dnl
            CPU_COUNT="1"
            AC_MSG_RESULT([[unable to detect (assuming 1)]])
          ])dnl
        ])dnl
      ])dnl

# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_func_posix_memalign.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_FUNC_POSIX_MEMALIGN
#
# DESCRIPTION
#
#   Some versions of posix_memalign (notably glibc 2.2.5) incorrectly apply
#   their power-of-two check to the size argument, not the alignment
#   argument. AX_FUNC_POSIX_MEMALIGN defines HAVE_POSIX_MEMALIGN if the
#   power-of-two check is correctly applied to the alignment argument.
#
# LICENSE
#
#   Copyright (c) 2008 Scott Pakin <pakin@uiuc.edu>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 9

AC_DEFUN([AX_FUNC_POSIX_MEMALIGN],
[AC_CACHE_CHECK([for working posix_memalign],
  [ax_cv_func_posix_memalign_works],
  [AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int
main ()
{
  void *buffer;

  /* Some versions of glibc incorrectly perform the alignment check on
   * the size word. */
  exit (posix_memalign (&buffer, sizeof(void *), 123) != 0);
}
    ]])],
    [ax_cv_func_posix_memalign_works=yes],
    [ax_cv_func_posix_memalign_works=no],
    [ax_cv_func_posix_memalign_works=no])])
if test "$ax_cv_func_posix_memalign_works" = "yes" ; then
  AC_DEFINE([HAVE_POSIX_MEMALIGN], [1],
    [Define to 1 if `posix_memalign' works.])
fi
])

# ===========================================================================
#      https://www.gnu.org/software/autoconf-archive/ax_is_release.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_IS_RELEASE(POLICY)
#
# DESCRIPTION
#
#   Determine whether the code is being configured as a release, or from
#   git. Set the ax_is_release variable to 'yes' or 'no'.
#
#   If building a release version, it is recommended that the configure
#   script disable compiler errors and debug features, by conditionalising
#   them on the ax_is_release variable.  If building from git, these
#   features should be enabled.
#
#   The POLICY parameter specifies how ax_is_release is determined. It can
#   take the following values:
#
#    * git-directory:  ax_is_release will be 'no' if a '.git' directory exists
#    * minor-version:  ax_is_release will be 'no' if the minor version number
#                      in $PACKAGE_VERSION is odd; this assumes
#                      $PACKAGE_VERSION follows the 'major.minor.micro' scheme
#    * micro-version:  ax_is_release will be 'no' if the micro version number
#                      in $PACKAGE_VERSION is odd; this assumes
#                      $PACKAGE_VERSION follows the 'major.minor.micro' scheme
#    * dash-version:   ax_is_release will be 'no' if there is a dash '-'
#                      in $PACKAGE_VERSION, for example 1.2-pre3, 1.2.42-a8b9
#                      or 2.0-dirty (in particular this is suitable for use
#                      with git-version-gen)
#    * always:         ax_is_release will always be 'yes'
#    * never:          ax_is_release will always be 'no'
#
#   Other policies may be added in future.
#
# LICENSE
#
#   Copyright (c) 2015 Philip Withnall <philip@tecnocode.co.uk>
#   Copyright (c) 2016 Collabora Ltd.
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.

#serial 7

AC_DEFUN([AX_IS_RELEASE],[
    AC_BEFORE([AC_INIT],[$0])

    m4_case([$1],
      [git-directory],[
        # $is_release = (.git directory does not exist)
        AS_IF([test -d ${srcdir}/.git],[ax_is_release=no],[ax_is_release=yes])
      ],
      [minor-version],[
        # $is_release = ($minor_version is even)
        minor_version=`echo "$PACKAGE_VERSION" | sed 's/[[^.]][[^.]]*.\([[^.]][[^.]]*\).*/\1/'`
        AS_IF([test "$(( $minor_version % 2 ))" -ne 0],
              [ax_is_release=no],[ax_is_release=yes])
      ],
      [micro-version],[
        # $is_release = ($micro_version is even)
        micro_version=`echo "$PACKAGE_VERSION" | sed 's/[[^.]]*\.[[^.]]*\.\([[^.]]*\).*/\1/'`
        AS_IF([test "$(( $micro_version % 2 ))" -ne 0],
              [ax_is_release=no],[ax_is_release=yes])
      ],
      [dash-version],[
        # $is_release = ($PACKAGE_VERSION has a dash)
        AS_CASE([$PACKAGE_VERSION],
                [*-*], [ax_is_release=no],
                [*], [ax_is_release=yes])
      ],
      [always],[ax_is_release=yes],
      [never],[ax_is_release=no],
      [
        AC_MSG_ERROR([Invalid policy. Valid policies: git-directory, minor-version, micro-version, dash-version, always, never.])
      ])
])

# ===========================================================================
#      https://www.gnu.org/software/autoconf-archive/ax_append_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_APPEND_FLAG(FLAG, [FLAGS-VARIABLE])
#
# DESCRIPTION
#
#   FLAG is appended to the FLAGS-VARIABLE shell variable, with a space
#   added in between.
#
#   If FLAGS-VARIABLE is not specified, the current language's flags (e.g.
#   CFLAGS) is used.  FLAGS-VARIABLE is not changed if it already contains
#   FLAG.  If FLAGS-VARIABLE is unset in the shell, it is set to exactly
#   FLAG.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 8

AC_DEFUN([AX_APPEND_FLAG],
[dnl
AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_SET_IF
AS_VAR_PUSHDEF([FLAGS], [m4_default($2,_AC_LANG_PREFIX[FLAGS])])
AS_VAR_SET_IF(FLAGS,[
  AS_CASE([" AS_VAR_GET(FLAGS) "],
    [*" $1 "*], [AC_RUN_LOG([: FLAGS already contains $1])],
    [
     AS_VAR_APPEND(FLAGS,[" $1"])
     AC_RUN_LOG([: FLAGS="$FLAGS"])
    ])
  ],
  [
  AS_VAR_SET(FLAGS,[$1])
  AC_RUN_LOG([: FLAGS="$FLAGS"])
  ])
AS_VAR_POPDEF([FLAGS])dnl
])dnl AX_APPEND_FLAG

# ============================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_append_compile_flags.html
# ============================================================================
#
# SYNOPSIS
#
#   AX_APPEND_COMPILE_FLAGS([FLAG1 FLAG2 ...], [FLAGS-VARIABLE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   For every FLAG1, FLAG2 it is checked whether the compiler works with the
#   flag.  If it does, the flag is added FLAGS-VARIABLE
#
#   If FLAGS-VARIABLE is not specified, the current language's flags (e.g.
#   CFLAGS) is used.  During the check the flag is always added to the
#   current language's flags.
#
#   If EXTRA-FLAGS is defined, it is added to the current language's default
#   flags (e.g. CFLAGS) when the check is done.  The check is thus made with
#   the flags: "CFLAGS EXTRA-FLAGS FLAG".  This can for example be used to
#   force the compiler to issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_COMPILE_IFELSE.
#
#   NOTE: This macro depends on the AX_APPEND_FLAG and
#   AX_CHECK_COMPILE_FLAG. Please keep this macro in sync with
#   AX_APPEND_LINK_FLAGS.
#
# LICENSE
#
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 7

AC_DEFUN([AX_APPEND_COMPILE_FLAGS],
[AX_REQUIRE_DEFINED([AX_CHECK_COMPILE_FLAG])
AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
for flag in $1; do
  AX_CHECK_COMPILE_FLAG([$flag], [AX_APPEND_FLAG([$flag], [$2])], [], [$3], [$4])
done
])dnl AX_APPEND_COMPILE_FLAGS

# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_require_defined.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_REQUIRE_DEFINED(MACRO)
#
# DESCRIPTION
#
#   AX_REQUIRE_DEFINED is a simple helper for making sure other macros have
#   been defined and thus are available for use.  This avoids random issues
#   where a macro isn't expanded.  Instead the configure script emits a
#   non-fatal:
#
#     ./configure: line 1673: AX_CFLAGS_WARN_ALL: command not found
#
#   It's like AC_REQUIRE except it doesn't expand the required macro.
#
#   Here's an example:
#
#     AX_REQUIRE_DEFINED([AX_CHECK_LINK_FLAG])
#
# LICENSE
#
#   Copyright (c) 2014 Mike Frysinger <vapier@gentoo.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 2

AC_DEFUN([AX_REQUIRE_DEFINED], [dnl
  m4_ifndef([$1], [m4_fatal([macro ]$1[ is not defined; is a m4 file missing?])])
])dnl AX_REQUIRE_DEFINED

# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_check_compile_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_COMPILE_FLAG(FLAG, [ACTION-SUCCESS], [ACTION-FAILURE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   Check whether the given FLAG works with the current language's compiler
#   or gives an error.  (Warnings, however, are ignored)
#
#   ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
#   success/failure.
#
#   If EXTRA-FLAGS is defined, it is added to the current language's default
#   flags (e.g. CFLAGS) when the check is done.  The check is thus made with
#   the flags: "CFLAGS EXTRA-FLAGS FLAG".  This can for example be used to
#   force the compiler to issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_COMPILE_IFELSE.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION. Please keep this
#   macro in sync with AX_CHECK_{PREPROC,LINK}_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

# ===========================================================================
#   https://www.gnu.org/software/autoconf-archive/ax_append_link_flags.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_APPEND_LINK_FLAGS([FLAG1 FLAG2 ...], [FLAGS-VARIABLE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   For every FLAG1, FLAG2 it is checked whether the linker works with the
#   flag.  If it does, the flag is added FLAGS-VARIABLE
#
#   If FLAGS-VARIABLE is not specified, the linker's flags (LDFLAGS) is
#   used. During the check the flag is always added to the linker's flags.
#
#   If EXTRA-FLAGS is defined, it is added to the linker's default flags
#   when the check is done.  The check is thus made with the flags: "LDFLAGS
#   EXTRA-FLAGS FLAG".  This can for example be used to force the linker to
#   issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_COMPILE_IFELSE.
#
#   NOTE: This macro depends on the AX_APPEND_FLAG and AX_CHECK_LINK_FLAG.
#   Please keep this macro in sync with AX_APPEND_COMPILE_FLAGS.
#
# LICENSE
#
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 7

AC_DEFUN([AX_APPEND_LINK_FLAGS],
[AX_REQUIRE_DEFINED([AX_CHECK_LINK_FLAG])
AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
for flag in $1; do
  AX_CHECK_LINK_FLAG([$flag], [AX_APPEND_FLAG([$flag], [m4_default([$2], [LDFLAGS])])], [], [$3], [$4])
done
])dnl AX_APPEND_LINK_FLAGS


# ==============================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_compiler_flags_ldflags.html
# ==============================================================================
#
# SYNOPSIS
#
#   AX_COMPILER_FLAGS_LDFLAGS([VARIABLE], [IS-RELEASE], [EXTRA-BASE-FLAGS], [EXTRA-YES-FLAGS])
#
# DESCRIPTION
#
#   Add warning flags for the linker to VARIABLE, which defaults to
#   WARN_LDFLAGS.  VARIABLE is AC_SUBST-ed by this macro, but must be
#   manually added to the LDFLAGS variable for each target in the code base.
#
#   This macro depends on the environment set up by AX_COMPILER_FLAGS.
#   Specifically, it uses the value of $ax_enable_compile_warnings to decide
#   which flags to enable.
#
# LICENSE
#
#   Copyright (c) 2014, 2015 Philip Withnall <philip@tecnocode.co.uk>
#   Copyright (c) 2017, 2018 Reini Urban <rurban@cpan.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 9

AC_DEFUN([AX_COMPILER_FLAGS_LDFLAGS],[
    AX_REQUIRE_DEFINED([AX_APPEND_LINK_FLAGS])
    AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
    AX_REQUIRE_DEFINED([AX_CHECK_COMPILE_FLAG])
    AX_REQUIRE_DEFINED([AX_CHECK_LINK_FLAG])

    # Variable names
    m4_define([ax_warn_ldflags_variable],
              [m4_normalize(ifelse([$1],,[WARN_LDFLAGS],[$1]))])

    # Always pass -Werror=unknown-warning-option to get Clang to fail on bad
    # flags, otherwise they are always appended to the warn_ldflags variable,
    # and Clang warns on them for every compilation unit.
    # If this is passed to GCC, it will explode, so the flag must be enabled
    # conditionally.
    AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option],[
        ax_compiler_flags_test="-Werror=unknown-warning-option"
    ],[
        ax_compiler_flags_test=""
    ])

    AX_CHECK_LINK_FLAG([-Wl,--as-needed], [
        AX_APPEND_LINK_FLAGS([-Wl,--as-needed],
          [AM_LDFLAGS],[$ax_compiler_flags_test])
    ])
    AX_CHECK_LINK_FLAG([-Wl,-z,relro], [
        AX_APPEND_LINK_FLAGS([-Wl,-z,relro],
          [AM_LDFLAGS],[$ax_compiler_flags_test])
    ])
    AX_CHECK_LINK_FLAG([-Wl,-z,now], [
        AX_APPEND_LINK_FLAGS([-Wl,-z,now],
          [AM_LDFLAGS],[$ax_compiler_flags_test])
    ])
    AX_CHECK_LINK_FLAG([-Wl,-z,noexecstack], [
        AX_APPEND_LINK_FLAGS([-Wl,-z,noexecstack],
          [AM_LDFLAGS],[$ax_compiler_flags_test])
    ])
    # textonly, retpolineplt not yet

    # macOS and cygwin linker do not have --as-needed
    AX_CHECK_LINK_FLAG([-Wl,--no-as-needed], [
        ax_compiler_flags_as_needed_option="-Wl,--no-as-needed"
    ], [
        ax_compiler_flags_as_needed_option=""
    ])

    # macOS linker speaks with a different accent
    ax_compiler_flags_fatal_warnings_option=""
    AX_CHECK_LINK_FLAG([-Wl,--fatal-warnings], [
        ax_compiler_flags_fatal_warnings_option="-Wl,--fatal-warnings"
    ])
    AX_CHECK_LINK_FLAG([-Wl,-fatal_warnings], [
        ax_compiler_flags_fatal_warnings_option="-Wl,-fatal_warnings"
    ])

    # Base flags
    AX_APPEND_LINK_FLAGS([ dnl
        $ax_compiler_flags_as_needed_option dnl
        $3 dnl
    ],ax_warn_ldflags_variable,[$ax_compiler_flags_test])

    AS_IF([test "$ax_enable_compile_warnings" != "no"],[
        # "yes" flags
        AX_APPEND_LINK_FLAGS([$4 $5 $6 $7],
                                ax_warn_ldflags_variable,
                                [$ax_compiler_flags_test])
    ])
    AS_IF([test "$ax_enable_compile_warnings" = "error"],[
        # "error" flags; -Werror has to be appended unconditionally because
        # it's not possible to test for
        #
        # suggest-attribute=format is disabled because it gives too many false
        # positives
        AX_APPEND_LINK_FLAGS([ dnl
            $ax_compiler_flags_fatal_warnings_option dnl
        ],ax_warn_ldflags_variable,[$ax_compiler_flags_test])
    ])

    # Substitute the variables
    AC_SUBST(ax_warn_ldflags_variable)
])dnl AX_COMPILER_FLAGS


# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_add_fortify_source.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_ADD_FORTIFY_SOURCE
#
# DESCRIPTION
#
#   Check whether -D_FORTIFY_SOURCE=2 can be added to CPPFLAGS without macro
#   redefinition warnings. Some distributions (such as Gentoo Linux) enable
#   _FORTIFY_SOURCE globally in their compilers, leading to unnecessary
#   warnings in the form of
#
#     <command-line>:0:0: error: "_FORTIFY_SOURCE" redefined [-Werror]
#     <built-in>: note: this is the location of the previous definition
#
#   which is a problem if -Werror is enabled. This macro checks whether
#   _FORTIFY_SOURCE is already defined, and if not, adds -D_FORTIFY_SOURCE=2
#   to CPPFLAGS.
#
# LICENSE
#
#   Copyright (c) 2017 David Seifert <soap@gentoo.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 2

AC_DEFUN([AX_ADD_FORTIFY_SOURCE],[
    AC_MSG_CHECKING([whether to add -D_FORTIFY_SOURCE=2 to CPPFLAGS])
    AC_LINK_IFELSE([
        AC_LANG_SOURCE(
            [[
                int main() {
                #ifndef _FORTIFY_SOURCE
                    return 0;
                #else
                    this_is_an_error;
                #endif
                }
            ]]
        )], [
            AC_MSG_RESULT([yes])
            CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=2"
        ], [
            AC_MSG_RESULT([no])
    ])
])
# ===========================================================================
#   https://www.gnu.org/software/autoconf-archive/ax_prog_perl_version.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PROG_PERL_VERSION([VERSION],[ACTION-IF-TRUE],[ACTION-IF-FALSE])
#
# DESCRIPTION
#
#   Makes sure that perl supports the version indicated. If true the shell
#   commands in ACTION-IF-TRUE are executed. If not the shell commands in
#   ACTION-IF-FALSE are run. Note if $PERL is not set (for example by
#   running AC_CHECK_PROG or AC_PATH_PROG) the macro will fail.
#
#   Example:
#
#     AC_PATH_PROG([PERL],[perl])
#     AX_PROG_PERL_VERSION([5.8.0],[ ... ],[ ... ])
#
#   This will check to make sure that the perl you have supports at least
#   version 5.8.0.
#
#   NOTE: This macro uses the $PERL variable to perform the check.
#   AX_WITH_PERL can be used to set that variable prior to running this
#   macro. The $PERL_VERSION variable will be valorized with the detected
#   version.
#
# LICENSE
#
#   Copyright (c) 2009 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 13

AC_DEFUN([AX_PROG_PERL_VERSION],[
    AC_REQUIRE([AC_PROG_SED])
    AC_REQUIRE([AC_PROG_GREP])

    AS_IF([test -n "$PERL"],[
        ax_perl_version="$1"

        AC_MSG_CHECKING([for perl version])
        changequote(<<,>>)
        perl_version=`$PERL --version 2>&1 \
          | $SED -n -e '/This is perl/b inspect
b
: inspect
s/.* (\{0,1\}v\([0-9]*\.[0-9]*\.[0-9]*\))\{0,1\} .*/\1/;p'`
        changequote([,])
        AC_MSG_RESULT($perl_version)

	AC_SUBST([PERL_VERSION],[$perl_version])

        AX_COMPARE_VERSION([$ax_perl_version],[le],[$perl_version],[
	    :
            $2
        ],[
	    :
            $3
        ])
    ],[
        AC_MSG_WARN([could not find the perl interpreter])
        $3
    ])
])
