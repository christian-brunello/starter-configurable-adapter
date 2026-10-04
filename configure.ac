#
# starter-configurable-adapter - configure.ac
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# Process this file with autoconf to produce configure.

AC_INIT([starter-configurable-adapter], [1.0.0], [startercontroller@gmail.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/autoconf.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])

AC_PROG_CC
AM_PROG_AR

LT_INIT([dlopen])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.30 gobject-2.0 >= 2.30 gio-2.0 >= 2.30 gmodule-2.0])
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0 >= 2.9.0])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0])

# GLib APIs added after Ubuntu 20.04's 2.64 (e.g. g_string_replace @ 2.68)
saved_CFLAGS=$CFLAGS
saved_LIBS=$LIBS
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LIBS="$LIBS $GLIB_LIBS"
AC_CHECK_FUNCS([g_string_replace])
CFLAGS=$saved_CFLAGS
LIBS=$saved_LIBS

# Prefer libsoup-3.0, fall back to libsoup-2.4
have_libsoup3=no
PKG_CHECK_MODULES([LIBSOUP], [libsoup-3.0],
  [have_libsoup3=yes],
  [PKG_CHECK_MODULES([LIBSOUP], [libsoup-2.4 >= 2.58])])

AM_CONDITIONAL([HAVE_LIBSOUP_3], [test "$have_libsoup3" = "yes"])
AS_IF([test "$have_libsoup3" = "yes"],
  [AC_DEFINE([HAVE_LIBSOUP_3], [1], [Define if building against libsoup 3.x])])

STCA_CFLAGS="$GLIB_CFLAGS $LIBXML2_CFLAGS $JSON_GLIB_CFLAGS $LIBSOUP_CFLAGS"
STCA_LIBS="$GLIB_LIBS $LIBXML2_LIBS $JSON_GLIB_LIBS $LIBSOUP_LIBS"

AC_SUBST(STCA_CFLAGS)
AC_SUBST(STCA_LIBS)

AC_PATH_TOOL([MAKEINFO], [makeinfo], [:])
if test "$MAKEINFO" = ":" ; then
    AC_MSG_ERROR([makeinfo not found. Please install it and try again.])
fi

AC_CONFIG_FILES([
  Makefile
  include/Makefile
  lib/Makefile
  src/Makefile
  plugins/Makefile
  plugins/dummy/Makefile
  examples/Makefile
  examples/dummy/Makefile
  doc/Makefile
  starter-configurable-adapter.pc
])

AC_OUTPUT

AS_IF([test "$have_libsoup3" = "yes"],
  [soup_msg="libsoup-3.0"],
  [soup_msg="libsoup-2.4"])
AC_MSG_NOTICE([HTTP server will use $soup_msg])
