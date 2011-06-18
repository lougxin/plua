dnl $Id$
PHP_ARG_WITH(plua, for lua support,
[  --with-plua=[DIR]    Include php lua support])

if test "$PHP_PLUA" != "no"; then
  if test -r $PHP_PLUA/include/lua.h; then
    LUA_DIR=$PHP_PLUA
  else
    AC_MSG_CHECKING(for lua in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/lua/include.h; then
        LUA_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$LUA_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the lua distribution - lua.h should be in <lua-dir>/include/)
  fi

  LUA_LIB_NAME=lua

  if test -r $PHP_PLUA/$PHP_LIBDIR/lib${LUA_LIB_NAME}.a; then
    LUA_LIB_DIR=$PHP_PLUA/$PHP_LIBDIR
  else
    AC_MSG_CHECKING(for lua library in default path)
    for i in /usr/lib /usr/lib64; do
      if test -r $i/$PHP_LIBDIR/${LUA_LIB_DIR}.a; then
        LUA_LIB_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$LUA_LIB_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the lua distribution - lua library should be in <lua-dir>/lib/)
  fi

  PHP_ADD_INCLUDE($LUA_DIR/include)
  PHP_ADD_LIBRARY_WITH_PATH(lua, $LUA_LIB_DIR, PLUA_SHARED_LIBADD)
  PHP_SUBST(PLUA_SHARED_LIBADD)
  PHP_NEW_EXTENSION(plua, plua.c, $ext_shared)
fi
