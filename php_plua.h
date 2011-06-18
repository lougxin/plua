/*
  +----------------------------------------------------------------------+
  | PLua                                                                 |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: laruence@gmail.com                                           |
  |         Johannes Schlueter  <johannes@php.net>                       |
  |         Marcelo  Araujo     <msaraujo@php.net>                       |
  |         Andreas Streichardt <andreas.streichardt@globalpark.com      |
  +----------------------------------------------------------------------+
  $Id$ 
 */

#ifndef PHP_PLUA_H
#define PHP_PLUA_H

extern zend_module_entry plua_module_entry;
#define phpext_lua_ptr &lua_module_entry

#ifdef PHP_WIN32
#define PHP_PLUA_API __declspec(dllexport)
#else
#define PHP_PLUA_API
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifdef ZTS
#include "TSRM.h"
#endif

struct _php_plua_object {
  zend_object obj;
  lua_State *L;
};

typedef struct _php_plua_object php_plua_object;

PHP_MINIT_FUNCTION(plua);
PHP_MSHUTDOWN_FUNCTION(plua);
PHP_RINIT_FUNCTION(plua);
PHP_RSHUTDOWN_FUNCTION(plua);
PHP_MINFO_FUNCTION(plua);


PHP_METHOD(plua, __construct);
PHP_METHOD(plua, eval);
PHP_METHOD(plua, require);

#define Z_LUAVAL_P(obj) ((php_plua_object*)(zend_object_store_get_object(obj)))->L

#if defined(PHP_WIN32) && (_MSC_VER > 1400)

#define plua_error(format, ...) \
 		php_error(E_USER_ERROR, format, __VA_ARGS__);

#define plua_strict(format, ...) \
		php_error(E_STRICT, format,  __VA_ARGS__); 

#define plua_warn(format, ...) \
		php_error(E_WARNING, format,  __VA_ARGS__); 

#define plua_notice(format, ...) \
		php_error(E_NOTICE, format,  __VA_ARGS__);

#elif defined(__GNUC__) 

#define plua_error(format, arg...) \
 		php_error(E_USER_ERROR, format, ##arg);

#define plua_strict(format, arg...) \
		php_error(E_STRICT, format,  ##arg); 

#define plua_warn(format, arg...) \
		php_error(E_WARNING, format,  ##arg); 

#define plua_notice(format, arg...) \
		php_error(E_NOTICE, format,  ##arg);
#endif

#endif	/* PHP_PLUA_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
