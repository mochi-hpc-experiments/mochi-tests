#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([sds-tests], [0.1], [],[],[]) 
AC_CONFIG_MACRO_DIR([m4])
LT_INIT

AC_CANONICAL_TARGET
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

# we should remove this soon, only needed for automake 1.10 and older
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([README.md])
AC_CONFIG_HEADERS([sds-tests-config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CXXCPP

AC_PROG_MKDIR_P
AC_REQUIRE_CPP

AC_CHECK_SIZEOF([long int])

dnl
dnl Verify pkg-config
dnl
PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG" == "x"; then
   AC_MSG_ERROR([Could not find pkg-config utility!])
fi

PKG_CHECK_MODULES([MARGO],[margo],[],
   [AC_MSG_ERROR([Could not find working margo installation!])])
LIBS="$MARGO_LIBS $LIBS"
CPPFLAGS="$MARGO_CFLAGS $CPPFLAGS"
CFLAGS="$MARGO_CFLAGS $CFLAGS"

PKG_CHECK_MODULES([SSG],[ssg],[],
   [AC_MSG_ERROR([Could not find working ssg installation!])])
LIBS="$SSG_LIBS $LIBS"
CPPFLAGS="$SSG_CFLAGS $CPPFLAGS"
CFLAGS="$SSG_CFLAGS $CFLAGS"

AC_CHECK_HEADERS(ssg-mpi.h,[],
  [AC_MSG_ERROR([Could not find ssg-mpi.h for MPI support in
  SSG])])

PKG_CHECK_MODULES([BAKESERVER],[bake-server],[],
        AC_MSG_ERROR([Could not find working BAKE server installation!]) )
LIBS="$BAKESERVER_LIBS $LIBS"
CPPFLAGS="$BAKESERVER_CFLAGS $CPPFLAGS"
CFLAGS="$BAKESERVER_CFLAGS $CFLAGS"

PKG_CHECK_MODULES([BAKECLIENT],[bake-client],[],
        AC_MSG_ERROR([Could not find working BAKE client installation!]) )
LIBS="$BAKECLIENT_LIBS $LIBS"
CPPFLAGS="$BAKECLIENT_CFLAGS $CPPFLAGS"
CFLAGS="$BAKECLIENT_CFLAGS $CFLAGS"

AC_MSG_CHECKING([If MPI programs can be compiled])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include<mpi.h>]], [[MPI_Init(0,0);]])],
    [AC_DEFINE([HAVE_MPI], [1], [Define to 1 if compiled with MPI support])
     AC_MSG_RESULT([yes])],
    [AC_MSG_ERROR([Unable to compile MPI programs, try CC=mpicc])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
 
