dnl $Id$
dnl config.m4 for extension mymonitor

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(mymonitor, for mymonitor support,
dnl Make sure that the comment is aligned:
dnl [  --with-mymonitor             Include mymonitor support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(mymonitor, whether to enable mymonitor support,
dnl Make sure that the comment is aligned:
[  --enable-mymonitor           Enable mymonitor support])

if test "$PHP_MYMONITOR" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-mymonitor -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/mymonitor.h"  # you most likely want to change this
  dnl if test -r $PHP_MYMONITOR/$SEARCH_FOR; then # path given as parameter
  dnl   MYMONITOR_DIR=$PHP_MYMONITOR
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for mymonitor files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       MYMONITOR_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$MYMONITOR_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the mymonitor distribution])
  dnl fi

  dnl # --with-mymonitor -> add include path
  dnl PHP_ADD_INCLUDE($MYMONITOR_DIR/include)

  dnl # --with-mymonitor -> check for lib and symbol presence
  dnl LIBNAME=mymonitor # you may want to change this
  dnl LIBSYMBOL=mymonitor # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $MYMONITOR_DIR/lib, MYMONITOR_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_MYMONITORLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong mymonitor lib version or lib not found])
  dnl ],[
  dnl   -L$MYMONITOR_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(MYMONITOR_SHARED_LIBADD)

  PHP_NEW_EXTENSION(mymonitor, mymonitor.c, $ext_shared)
fi
