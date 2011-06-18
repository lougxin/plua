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
  |			Johannes Schlueter  <johannes@php.net>                       |
  |         Marcelo  Araujo     <msaraujo@php.net>                       |
  |         Andreas Streichardt <andreas.streichardt@globalpark.com      |
  +----------------------------------------------------------------------+
  $Id$ 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "lua.h"
#include "php_plua.h"

static zend_class_entry 	*plua_ce;
static zend_object_handlers lua_object_handlers;

/** {{{ ARG_INFO
 *
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_plua_call, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_plua_assign, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_plua_register, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, function)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_plua_include, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_plua_eval, 0, 0, 1)
	ZEND_ARG_INFO(0, statements)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ lua_functions[]
 *
 */
zend_function_entry plua_functions[] = {
	{NULL, NULL, NULL}
};

/* }}} */

/* {{{ lua_module_entry
 */
zend_module_entry plua_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"plua",
	plua_functions,
	PHP_MINIT(plua),
	PHP_MSHUTDOWN(plua),
	PHP_RINIT(plua),		
	PHP_RSHUTDOWN(plua),
	PHP_MINFO(plua),
#if ZEND_MODULE_API_NO >= 20010901
	"1.0.0", 
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PLUA
ZEND_GET_MODULE(plua)
#endif

/** {{{ function previos declare
 */
static zval * plua_get_zval_from_lua(lua_State *L, int index TSRMLS_DC);
/* }}} */

/** {{{ static void plua_stack_dump(lua_State* L)
 *  just for debug
 */
#ifdef PHP_PLUA_DEBUG
static void plua_stack_dump(lua_State* L) {
	int i = 1;
	int n = lua_gettop(L); 
	printf("The Length of stack is %d\n", n);
	for (; i <= n; ++i) {
		int t = lua_type(L, i);
		printf("%s:", lua_typename(L, t)); 
		switch(t) {
			case LUA_TNUMBER:
				printf("%f", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("%s", lua_tostring(L, i));
				break;
			case LUA_TTABLE:
				break;
			case LUA_TFUNCTION:
				break;
			case LUA_TNIL:
				printf("NULL");
				break;
			case LUA_TBOOLEAN:
				printf("%s", lua_toboolean(L, i) ? "TRUE" : "FALSE");
				break;
			default:
				break;
		}
		printf("\n");
	}
}
#endif
/* }}} */

/** {{{ static int plua_atpanic(lua_State *L)
 */
static int plua_atpanic(lua_State *L) { 
  TSRMLS_FETCH();
  plua_error("lua panic (%s)", lua_tostring(L, 1));
  lua_pop(L, 1);
  zend_bailout();
  return 0;
} 
/* }}} */

/** {{{ static int plua_print(lua_State *L)
 */
static int plua_print(lua_State *L)  {
  int i = 0;
  for (i = -lua_gettop(L) ; i<0; i++) {
	  zval *tmp = plua_get_zval_from_lua(L, i TSRMLS_CC);
	  zend_print_zval_r(tmp, 1 TSRMLS_CC);
  }
  return 0;
} 
/* }}} */

/** {{{ static void * plua_alloc_function(void *ud, void *ptr, size_t osize, size_t nsize)
 *
 * the memory-allocation function used by Lua states.
 */
static void * plua_alloc_function(void *ud, void *ptr, size_t osize, size_t nsize) {
	(void) osize;
	(void) nsize;

	if (nsize) {
		if (ptr) {
			return erealloc(ptr, nsize);
		} 
		return emalloc(nsize);
	} else {
		if (ptr) {
			efree(ptr);
		}
	}

	return NULL;
}
/* }}} */

/** {{{ static void plua_dtor_object(void *object, zend_object_handle handle TSRMLS_DC)
 *  the dtor function for lua object
 */
static void plua_dtor_object(void *object, zend_object_handle handle TSRMLS_DC) {
	php_plua_object *lua_obj = (php_plua_object *)object;

	zend_object_std_dtor(&(lua_obj->obj) TSRMLS_CC);

	if (lua_obj->L) {
		lua_close(lua_obj->L);
	}

	efree(lua_obj);
} 
/* }}} */

/** {{{ static zend_object_value plua_create_object(zend_class_entry *ce TSRMLS_DC) 
 *
 * the create object handler for lua
 */
static zend_object_value plua_create_object(zend_class_entry *ce TSRMLS_DC) {
	zend_object_value obj 	 = {0};
	php_plua_object 	*lua_obj = NULL;
	lua_State 		*L	 	 = NULL;

	L = lua_newstate(plua_alloc_function, NULL);
	lua_atpanic(L, plua_atpanic);

	lua_obj = emalloc(sizeof(php_plua_object));
	lua_obj->L = L;
	zend_object_std_init(&(lua_obj->obj), ce TSRMLS_CC);
	zend_hash_copy(lua_obj->obj.properties,
			&ce->default_properties, (copy_ctor_func_t) zval_add_ref,
			(void *)0, sizeof(zval *));

	obj.handle   = zend_objects_store_put(lua_obj, plua_dtor_object, NULL, NULL TSRMLS_CC);
	obj.handlers = &lua_object_handlers;

	return obj;
} 
/* }}} */

/** {{{ static zval * plua_get_zval_from_lua(lua_State *L, int index TSRMLS_DC)
*/
static zval * plua_get_zval_from_lua(lua_State *L, int index TSRMLS_DC) {
	zval *retval = NULL;

	MAKE_STD_ZVAL(retval);
	ZVAL_NULL(retval);

	switch (lua_type(L, index)) {
		case LUA_TNIL:
			ZVAL_NULL(retval);
			break;
		case LUA_TBOOLEAN:
			ZVAL_BOOL(retval, lua_toboolean(L, index));
			break;
		case LUA_TNUMBER:
			ZVAL_DOUBLE(retval, lua_tonumber(L, index));
			break;
		case LUA_TSTRING:
			{
				char *val  = NULL;
				size_t len = 0;

				val = (char *)lua_tolstring(L, index, &len);
				ZVAL_STRINGL(retval, val, len, 1);
			}
			break;
		case LUA_TTABLE:
			array_init(retval);
			lua_pushnil(L);  /* first key */
			while (lua_next(L, index-1) != 0) {
				zval *key = NULL;
				zval *val = NULL;

				/* uses 'key' (at index -2) and 'value' (at index -1) */
				key = plua_get_zval_from_lua(L, -2 TSRMLS_CC);
				val = plua_get_zval_from_lua(L, -1 TSRMLS_CC);

				switch(Z_TYPE_P(key)) {
					case IS_DOUBLE:
					case IS_LONG:
						add_index_zval(retval, Z_DVAL_P(key), val);
						break;
					case IS_STRING:
						add_assoc_zval(retval, Z_STRVAL_P(key), val);
						break;
					case IS_ARRAY:
					case IS_OBJECT:
					default:
						break;
				}
				lua_pop(L, 1); 
				zval_ptr_dtor(&key);
			}
			break;
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			plua_warn("unsupported type '%s' for php", lua_typename(L, index));
	}

	return retval;
} 
/* }}} */

/** {{{ static int plua_send_zval_to_lua(lua_State *L, zval *val TSRMLS_DC) 
 */
static int plua_send_zval_to_lua(lua_State *L, zval *val TSRMLS_DC) {

  switch (Z_TYPE_P(val)) {
    case IS_BOOL:
      lua_pushboolean(L, Z_BVAL_P(val));
      break;
    case IS_NULL:	   
      lua_pushnil(L);	   
      break;
    case IS_DOUBLE:
	  lua_pushnumber(L, Z_DVAL_P(val));
	  break;
	case IS_LONG:
	  lua_pushnumber(L, Z_LVAL_P(val));
	  break;
	case IS_STRING:
	  lua_pushlstring(L, Z_STRVAL_P(val), Z_STRLEN_P(val));
	  break;
	case IS_OBJECT:
	case IS_ARRAY:
	  {
		  HashTable *ht  		= NULL;
		  zval 		**ppzval 	= NULL;

		  ht = HASH_OF(val);

		  lua_newtable(L);
		  for(zend_hash_internal_pointer_reset(ht); 
				  zend_hash_get_current_data(ht, (void **)&ppzval) == SUCCESS; 
				  zend_hash_move_forward(ht)) {
			  char *key = NULL;
			  int  len  = 0;
			  long idx  = 0;
			  uint type = 0;
			  zval *zkey= NULL;

			  type = zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL);

			  if (HASH_KEY_IS_STRING == type) {
				  MAKE_STD_ZVAL(zkey);
				  ZVAL_STRINGL(zkey, key, len - 1, 1);
			  } else if (HASH_KEY_IS_LONG == type) {
				  if (idx == 0) {
					  plua_warn("try to pass an array index begin with 0 to lua, the index 0 will be discarded");
					  continue;
				  }

				  MAKE_STD_ZVAL(zkey);
				  ZVAL_LONG(zkey, idx);
			  }

			  plua_send_zval_to_lua(L, zkey TSRMLS_CC);
			  plua_send_zval_to_lua(L, *ppzval TSRMLS_CC);
			  lua_settable(L, -3);

			  zval_ptr_dtor(&zkey);
		  }
	  }
      break;
    default:
      plua_error("unsupported type `%s' for lua", zend_zval_type_name(val));
      lua_pushnil(L);
	  return 1;
  }

  return 0;
} 
/* }}} */

/*** {{{ static int plua_arg_apply_func(void *data, void *L TSRMLS_DC)
 */
static int plua_arg_apply_func(void *data, void *L TSRMLS_DC) {
  plua_send_zval_to_lua((lua_State*)L, *(zval**)data TSRMLS_CC);
  return ZEND_HASH_APPLY_KEEP;
} /* }}} */

/** {{{  static int plua_call_callback(lua_State *L)
 */
static int plua_call_callback(lua_State *L) {
  int  order		 = 0;
  zval *return_value = NULL;
  zval *callbacks	 = NULL;
  zval **func		 = NULL;

  order = lua_tonumber(L, lua_upvalueindex(1));

  MAKE_STD_ZVAL(return_value);
  callbacks = zend_read_static_property(plua_ce, ZEND_STRL("_callbacks"), 1 TSRMLS_CC);

  if (ZVAL_IS_NULL(callbacks)) {
	  return 0;
  }

  if (zend_hash_index_find(Z_ARRVAL_P(callbacks), order, (void **)&func) == FAILURE) {
	  return 0;
  }

  if (!zend_is_callable(*func, 0, NULL)) {
	  return 0;
  } else {
	  zval **params = NULL;
	  int  i 		= 0;
	  int  arg_num  = lua_gettop(L);

	  params = ecalloc(arg_num, sizeof(zval));

	  for (; i<arg_num; i++) {
		  MAKE_STD_ZVAL(params[i]);
		  params[i] = plua_get_zval_from_lua(L, -(arg_num-i) TSRMLS_CC);
	  }

	  call_user_function(EG(function_table), NULL, *func, return_value, arg_num, params TSRMLS_CC);

	  plua_send_zval_to_lua(L, return_value TSRMLS_CC);

	  for (i=0; i<arg_num; i++) {
		  zval_ptr_dtor(&params[i]);
	  }

	  efree(params);
	  zval_ptr_dtor(&return_value);

	  return 1;
  }
}
/* }}} */

/** {{{ static zval * plua_call_lua_function(lua_State *L, zval *func, zval *args, int use_self TSRMLS_DC)
 */
static zval * plua_call_lua_function(lua_State *L, zval *func, zval *args, int use_self TSRMLS_DC) {
	int bp 		= 0;
	int sp 		= 0;
	int arg_num = 0;
	zval *ret   = NULL;

	if (IS_ARRAY == Z_TYPE_P(func)) {
		zval **t = NULL;
		zval **f = NULL;
		if (zend_hash_index_find(Z_ARRVAL_P(func), 0, (void **)&t) == FAILURE
				|| zend_hash_index_find(Z_ARRVAL_P(func), 1, (void **)&f) == FAILURE) {
			plua_warn("invalid lua function, argument must be an array which contain two elements: array('table', 'method')");
			return NULL;
		}
		lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_PP(t));
		if (LUA_TTABLE != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			plua_warn("invalid lua table '%s'", Z_STRVAL_PP(t));
			return NULL;
		}
		bp = lua_gettop(L);
		lua_getfield(L, -1, Z_STRVAL_PP(f));
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -2);
			plua_warn("invalid lua table function '%s'.%s", Z_STRVAL_PP(t), Z_STRVAL_PP(f));
			return NULL;
		}
	} else if (IS_STRING == Z_TYPE_P(func)) {
		bp = lua_gettop(L);
		lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_P(func));
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			plua_warn("invalid lua callback '%s'", Z_STRVAL_P(func));
			return NULL;
		}
	}

	if (use_self) {
		lua_pushvalue(L, -2);
		lua_remove(L, -2);
		arg_num++;
	}

	if (args) {
		arg_num += zend_hash_num_elements(Z_ARRVAL_P(args));
		zend_hash_apply_with_argument(Z_ARRVAL_P(args), plua_arg_apply_func, (void *)L TSRMLS_CC);
	}

	if (lua_pcall(L, arg_num, LUA_MULTRET, 0) != 0) { 
		plua_warn("call to lua function %s failed", lua_tostring(L, -1));
		lua_pop(L, lua_gettop(L) - bp);
		return NULL;
	}

	sp = lua_gettop(L) - bp;

	MAKE_STD_ZVAL(ret);

	if (!sp) {
		ZVAL_NULL(ret);
	} else if (sp == 1) {
		ret = plua_get_zval_from_lua(L, -1 TSRMLS_CC);
	} else {
		int  i = 0;
		array_init(ret);
		for (i = -sp; i < 0; i++) {
			zval *tmp = plua_get_zval_from_lua(L, i TSRMLS_CC);
			add_next_index_zval(ret, tmp);
		}
	}

	lua_pop(L, sp);

	if (IS_ARRAY == Z_TYPE_P(func)) {
		lua_pop(L, -1);
	}

	return ret;
} /* }}} */

/** {{{ proto PLua::eval(string $lua_chunk)
 * eval a lua chunk
 */
PHP_METHOD(plua, eval) {
	lua_State *L	 = NULL;
	char *statements = NULL;
	long len		 = 0;

	L = Z_LUAVAL_P(getThis());
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &statements, &len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (luaL_loadbuffer(L, statements, len, "line") || lua_pcall(L, 0, LUA_MULTRET, 0)) {
		plua_error("lua error: %s", lua_tostring(L, -1));
		lua_pop(L, 1); 
		RETURN_FALSE;
	} else {
		zval *tmp 		= NULL;
		int   ret_count	= 0;
		int   i   		= 0;

		ret_count = lua_gettop(L);
		if (ret_count > 1) {
			array_init(return_value);

			for (i = -ret_count; i<0; i++) {
				tmp = plua_get_zval_from_lua(L, i TSRMLS_CC);
				add_next_index_zval(return_value, tmp);
			}

		} else if (ret_count) {
			return_value = plua_get_zval_from_lua(L, -1 TSRMLS_CC);
		}
		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto PLua::include(string $file)
 * run a lua script file
 */
PHP_METHOD(plua, include) {
	lua_State *L = NULL;
	char *file   = NULL;
	long len     = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (php_check_open_basedir(file TSRMLS_CC) 
			|| (PG(safe_mode) && !php_checkuid(file, "rb+", CHECKUID_CHECK_MODE_PARAM))) {
		RETURN_FALSE;
	}

	L = Z_LUAVAL_P(getThis());

	if (luaL_dofile(L, file)) {
		plua_error("lua error: %s", lua_tostring(L, -1)); 
		lua_pop(L, 1);
		RETURN_FALSE;
	}  else {
		zval *tmp 		= NULL;
		int   ret_count	= 0;
		int   i   		= 0;

		ret_count = lua_gettop(L);
		if (ret_count > 1) {
			array_init(return_value);

			for (i = -ret_count; i<0; i++) {
				tmp = plua_get_zval_from_lua(L, i TSRMLS_CC);
				add_next_index_zval(return_value, tmp);
			}

		} else if (ret_count) {
			return_value = plua_get_zval_from_lua(L, -1 TSRMLS_CC);
		}

		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto PLua::__call(string $function, array $args) 
*/
PHP_METHOD(plua, __call) {
	long u_self = 0;
	zval *args	= NULL;
	zval *func  = NULL;
	lua_State *L = NULL;
	zval *ret   = NULL;

	L = Z_LUAVAL_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|al", &func, &args, &u_self) == FAILURE) {
		WRONG_PARAM_COUNT;
	} 

	if ((ret = plua_call_lua_function(L, func, args, u_self TSRMLS_CC))) {
		RETURN_ZVAL(ret, 1, 0);
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto PLua::call(string $function, array $args, bool $use_self) 
*/
PHP_METHOD(plua, call) {
	PHP_MN(plua___call)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/** {{{ proto PLua::assign(string $name, mix $value) 
*/
PHP_METHOD(plua, assign) {
	char *name   = NULL;
	zval *value	 = NULL;
	lua_State *L = NULL;
	int  len 	 = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &len, &value) == FAILURE) {
		WRONG_PARAM_COUNT;
	}     

	L = Z_LUAVAL_P(getThis());

	plua_send_zval_to_lua(L, value TSRMLS_CC);
	lua_setfield(L, LUA_GLOBALSINDEX, name);

	RETURN_TRUE;
}
/* }}} */

/** {{{ proto PLua::register(string $name, mix $value) 
*/
PHP_METHOD(plua, register) {
	zval *func = NULL;
	char *name = NULL;
	long len   = 0;

	lua_State *L    = NULL;
	zval* callbacks = NULL;

	L = Z_LUAVAL_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"sz", &name, &len, &func) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

  	callbacks = zend_read_static_property(plua_ce, ZEND_STRL("_callbacks"), 1 TSRMLS_CC);

	if (ZVAL_IS_NULL(callbacks)) {
		array_init(callbacks);
	}

	if (zend_is_callable(func, 0, NULL)) {
		lua_pushnumber(L, zend_hash_num_elements(Z_ARRVAL_P(callbacks)));
		lua_pushcclosure(L, plua_call_callback, 1);
		lua_setglobal(L, name);
	}

	zval_add_ref(&func);
	add_next_index_zval(callbacks, func);
	zend_update_static_property(plua_ce, ZEND_STRL("_callbacks"), callbacks TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/** {{{ proto PLua::__construct()
*/
PHP_METHOD(plua, __construct) {
	lua_State *L = Z_LUAVAL_P(getThis());
	luaL_openlibs(L);
	lua_register(L, "print", plua_print);
	if (ZEND_NUM_ARGS()) {
		PHP_MN(plua_include)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}
}
/* }}} */

/* {{{ plua_class_methods[]
 *
 */
zend_function_entry plua_class_methods[] = {
	PHP_ME(plua, __construct,   NULL,  					ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(plua, __call,		arginfo_plua_call,  	ZEND_ACC_PUBLIC)
	PHP_ME(plua, eval,          arginfo_plua_eval,  	ZEND_ACC_PUBLIC)
	PHP_ME(plua, include,		arginfo_plua_include, 	ZEND_ACC_PUBLIC)
	PHP_ME(plua, call,			arginfo_plua_call,  	ZEND_ACC_PUBLIC)
	PHP_ME(plua, assign,		arginfo_plua_assign,	ZEND_ACC_PUBLIC)
	PHP_ME(plua, register,		arginfo_plua_register, 	ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(plua) {
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "PLua", plua_class_methods);

	plua_ce = zend_register_internal_class(&ce TSRMLS_CC);
	plua_ce->create_object = plua_create_object;
	memcpy(&lua_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	plua_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_null(plua_ce, ZEND_STRL("_callbacks"), ZEND_ACC_STATIC | ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(plua)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(plua)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(plua)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(plua)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "plua support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
