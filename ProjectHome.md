Lua for PHP extension

**Plua has be moved to PECL**,  http://pecl.php.net/package/lua

PLua is a PHP extension which with the Lua interpreter embed in.

PLua is an rewritten version of PHP-Lua(http://phplua.3uu.de/), rewritten codes, add support for LUA\_TFUNCTION return by Lua, optimized profermance, made compatible with c89, and more other new features...

# APIS #
```
   $lua = new Plua($lua_script_file = NULL);
   
   $lua->eval("lua_statements");     //eval lua codes
   $lua->include("lua_script_file"); //import a lua script

   $lua->assign("name", $value); //assign a php variable to Lua
   $lua->register("name", $function); //register a PHP function to Lua with "name"

   $lua->call($string_lua_function_name, array() /*args*/);
   $lua->call($resouce_lua_anonymous_function, array() /*args);
   $lua->call(array("table", "method"), array(...., "push_self" => [true | false]) /*args*/);

   $lua->{$lua_function}(array()/*args*/);

   $lua->free($lua_closure_resource);
```

# Compile PLua in `*`nix #

```
$phpize
$configure --with-php-config=/path/to/php-config  --with-plua=/path/to/lua/
make && make install
```

# Compile PLua in Win32 #

Just define the CFLAGS = -I/path/to/lua/ -L/path/to/lua -llua

# Example #
**test.lua**
```
globalname = "From Lua";

function test()
    print(from);
    name = "laruence";
    comp = "baidu"
    callphp(name, comp);
    return (function() print(name, ":", globalname, "\n") end);
end
```

**lua.php**
```
<?php
$lua = new PLua("test.lua");

$a = new stdclass();
$a->foo = "bar";

class A {
   public static function intro($name, $company) {
    var_dump(func_get_args());
  }
}

$lua->assign("from", $a); /** assign a PHP var to Lua named from */

$lua->register("callphp", array("A", "intro")); /** register a php function to Lua named callphp */

$func = $lua->test(); /** call Lua function and get return closure */

var_dump($lua->globalname); /** echo a Lua variable named globalname */

$lua->globalname = "From PHP"; /** set a PHP string to Lua global variable named globalname */

$lua->call($func); /** call Lua function closure */

var_dump($lua->free($func)); /** free the Lua closure */

var_dump(Plua::LUA_VERSION); /** echo the Lua version */

var_dump($lua->call(array("string", "lower"), array("AAAA"))); /** call Lua table function string.lower */

var_dump($lua->call(array("table", "concat")
    , array(array(12,3,21,3,24,32,4,2,5,435,35), ","))); /** call Lua table function table.concat */
```

**output**
```
Array
 (
     [foo] => bar
 )
array(2) {
  [0]=>
  string(8) "laruence"
  [1]=>
  string(5) "baidu"
}
string(8) "From Lua"
laruence:From PHP
object(PLua)#1 (0) {
}
string(9) "Lua 5.1.4"
string(4) "aaaa"
PHP Warning:  try to pass an array index begin with 0 to lua, the index 0 will be discarded in *** on line 31
string(25) "3,21,3,24,32,4,2,5,435,35"
```