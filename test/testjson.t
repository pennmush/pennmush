login mortal
run tests:

$mortal->command('&json me={ "a": 1, "b": 2, "c": [1,2,3] }');

# isjson tests
test('json.isjson.1', $mortal, 'think isjson(v(json))', '1');
test('json.isjson.2', $mortal, 'think isjson(foo)', '0');

# Simple creation checks
test('json.null.1', $mortal, 'think json(null)', 'null');
test('json.null.2', $mortal, 'think json(null,null)', 'null');
test('json.null.3', $mortal, 'think json(null,foo)', '#-1');

test('json.boolean.1', $mortal, 'think json(boolean, true)', '^true$');
test('json.boolean.2', $mortal, 'think json(boolean, 1)', '^true$');
test('json.boolean.3', $mortal, 'think json(boolean, false)', '^false$');
test('json.boolean.4', $mortal, 'think json(boolean, 0)', '^false$');
test('json.boolean.5', $mortal, 'think json(boolean, 5)', '#-1');

test('json.string.1', $mortal, 'think json(string, foobar)', '^"foobar"$');
test('json.string.2', $mortal, 'think json(string, foo bar)', '^"foo bar"$');
test('json.string.3', $mortal, 'think json(string, foo "bar" baz)', '^"foo \\\\"bar\\\\" baz"$');
test('json.string.4', $mortal, 'think json(string, foo\\\\bar\\\\baz)', '^"foo\\\\\\\\bar\\\\\\\\baz"$');
test('json.string.5', $mortal, 'think json(string, accent(foo, f:o))', '^"f\\\\u00F6o"');
# Also tests unescape
test('json.string.6', $mortal, 'think json_query(json(string,accent(foo,f:o)), unescape)', '^f\xF6o');

test('json.number.1', $mortal, 'think json(number, 5)', '5');
test('json.number.2', $mortal, 'think json(number, 5.555)', '5.555');
test('json.number.3', $mortal, 'think json(number, foo)', '#-1');

test('json.array.1', $mortal, 'think json(array, "foo", 5, true)', '^\["foo",\s*5,\s*true\]$');
test('json.array.2', $mortal, 'think json(array, "foo", 5, json(array, "bar", 10))', '^\["foo",\s*5,\s*\["bar",\s*10\]\]$');

test('json.object.1', $mortal, 'think json(object, foo, 1, bar, "baz", boing, true)', '^{"foo":\s*1,\s*"bar":\s*"baz",\s*"boing":\s*true}$');
test('json.object.2', $mortal, 'think json(object, foo, 1, bar, "baz", boing, json(array, "nested", "test", 1)))', '^{"foo":\s*1,\s*"bar":\s*"baz",\s*"boing":\s*\["nested",\s*"test",\s*1\]}');

# json_query tests

# type
test('json.type.1', $mortal, 'think json_query("foo", type)', 'string');
test('json.type.2', $mortal, 'think json_query(1, type)', 'number');
test('json.type.3', $mortal, 'think json_query(3.14, type)', 'number');
test('json.type.4', $mortal, 'think json_query(null, type)', 'null');
test('json.type.5', $mortal, 'think json_query(true, type)', 'boolean');
test('json.type.6', $mortal, 'think json_query(false, type)', 'boolean');
test('json.type.7', $mortal, 'think json_query(v(json), type)', 'object');
test('json.type.8', $mortal, 'think json_query(json(array, 1), type)', 'array');
test('json.type.9', $mortal, 'think json_query(foo, type)', '#-1');

# size
test('json.size.1', $mortal, 'think json_query(null, size)', '0');
test('json.size.2', $mortal, 'think json_query(true, size)', '1');
test('json.size.3', $mortal, 'think json_query(1, size)', '1');
test('json.size.4', $mortal, 'think json_query(1.1, size)', '1');
test('json.size.5', $mortal, 'think json_query("foo", size)', '1');
test('json.size.6', $mortal, 'think json_query(\[\], size)', '0');
test('json.size.7', $mortal, 'think json_query(json(array, 1, 2), size)', '2');
test('json.size.8', $mortal, 'think json_query(\{\}, size)', '0');
test('json.size.9', $mortal, 'think json_query(v(json), size)', '3');

# exists
test('json.exists.1', $mortal, 'think json_query(v(json), exists, a)', '1');
test('json.exists.2', $mortal, 'think json_query(v(json), exists, d)', '0');
test('json.exists.3', $mortal, 'think json_query(v(json), exists, c, 1)', '1');
test('json.exists.4', $mortal, 'think json_query(v(json), exists, c, 3)', '0');
test('json.exists.5', $mortal, 'think json_query("foo", exists, a)', '#-1');
test('json.exists.6', $mortal, 'think json_query(foo, exists, a)', '#-1');

# get
test('json.get.1', $mortal, 'think json_query(v(json), get, a)', '1');
test('json.get.2', $mortal, 'think json_query(v(json), get, d)', '^$');
test('json.get.3', $mortal, 'think json_query(v(json), get, c, 1)', '2');
test('json.get.4', $mortal, 'think json_query(v(json), get, c, 3)', '^$');
test('json.get.5', $mortal, 'think json_query("foo", get, a)', '#-1');
test('json.get.6', $mortal, 'think json_query(foo, get, a)', '#-1');

# extract
test('json.extract.1', $mortal, 'think json_query(v(json), extract, $.a)', '1');
test('json.extract.2', $mortal, 'think json_query(v(json), extract, $.d)', '^$');
test('json.extract.3', $mortal, 'think json_query(v(json), extract, $.c\[1\])', '2');
test('json.extract.4', $mortal, 'think json_query(v(json), extract, $.c\[3\])', '^$');
test('json.extract.5', $mortal, 'think json_query("foo", extract, $)', 'foo');
test('json.extract.6', $mortal, 'think json_query(foo, extract, $.a)', '#-1');

# json_mod tests.

# set
test('json.set.1', $mortal, 'think json_mod(v(json), set, $.c, 3)', '"c":3');
test('json.set.2', $mortal, 'think json_mod(v(json), set, $.d, 3)', '"d":3');

# insert
test('json.insert.1', $mortal, 'think json_mod(v(json), insert, $.b, 3)', '"b":2');
test('json.insert.2', $mortal, 'think json_mod(v(json), insert, $.d, 3)', '"d":3');

# replace
test('json.replace.1', $mortal, 'think json_mod(v(json), replace, $.b, 3)', '"b":3');
test('json.replace.2', $mortal, 'think json_mod(v(json), replace, $.d, 3)', '!"d":3');

# remove
test('json.remove.1', $mortal, 'think json_mod(v(json), remove, $.c)', '\{"a":1,"b":2\}');
test('json.remove.2', $mortal, 'think json_mod(v(json), remove, $.d)', '\{"a":1,"b":2,"c":\[1,2,3\]\}');

# patch
test('json.patch.1', $mortal, "think json_mod(json(object,a,1,b,2),patch,json(object,c,3,d,4))", '^\{"a":1,"b":2,"c":3,"d":4\}');
test('json.patch.2', $mortal, "think json_mod(json(object,a,json(array,1,2),b,2), patch, json(object,a,9))", '^\{"a":9,"b":2\}');
test('json.patch.3', $mortal, 'think json_mod(json(object,a,json(array,1,2),b,2), patch, json(object, a, null))', '^\{"b":2\}');
test('json.patch.4', $mortal, 'think json_mod(json(object,a,1,b,2), patch, json(object,a,9,b,null,c,8))', '^\{"a":9,"c":8\}');
test('json.patch.5', $mortal, 'think json_mod(json(object,a,json(object,x,1,y,2),b,3), patch, json(object,a,json(object,y,9),c,8))', '^\{"a":\{"x":1,"y":9\},"b":3,"c":8\}');

# sort
test('json.sort.1', $mortal, 'think json_mod(json(array, json(object, id, 5), json(object, id, 4)), sort, $.id)', '^\[\{"id":4\},\{"id":5\}\]$');
test('json.sort.2', $mortal, 'think json_mod(json(array, json(object, id, "dog"), json(object, id, "cat")), sort, $.id)', '^\[\{"id":"cat"\},\{"id":"dog"\}\]$');
test('json.sort.3', $mortal, 'think json_mod(json(array, 5, 3, 1, 2), sort, $)', '^\[1,2,3,5\]$');
test('json.sort.4', $mortal, 'think json_mod(json(array, "e","m","a","z"), sort, $)', '^\["a","e","m","z"\]$');

# json_map

$mortal->command('&json_fn me=We got [art(%0)] %0: %1');
$mortal->command('&json2_fn me=%0:%1:%2');
$mortal->command('&json3_fn me=json_map(me/json4_fn, %1, @)');
$mortal->command('&json4_fn me=%1');

test('json.map.1', $mortal, 'think json_map(me/json_fn, "foo")', '^We got a string: foo$');
test('json.map.2', $mortal, 'think json_map(me/json_fn, \["foo"\, 5\], @)', '^We got a string: foo@We got a number: 5$');
test('json.map.3', $mortal, 'think json_map(me/json_fn, \["foo"\, \["bar"\, 10\]\], @)', '^We got a string: foo@We got an array: \["bar",10\]$');
test('json.map.4', $mortal, 'think json_map(me/json2_fn, json(object, a, 1, b, true, c, null), @)', '^number:1:a@boolean:true:b@null:null:c$');
# Test recursive calls
test('json.map.5', $mortal, 'think json_map(me/json3_fn, v(json), #)', '^1#2#1@2@3$');


