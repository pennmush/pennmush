login mortal
run tests:
# Simple type checks
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
test('json.string.6', $mortal, 'think json_query(json(string,accent(foo,f:o)), unescape)', '^f\xF6o');

test('json.number.1', $mortal, 'think json(number, 5)', '5');
test('json.number.2', $mortal, 'think json(number, 5.555)', '5.555');
test('json.number.3', $mortal, 'think json(number, foo)', '#-1');

test('json.array.1', $mortal, 'think json(array, "foo", 5, true)', '^\["foo", 5, true\]$');
test('json.array.2', $mortal, 'think json(array, "foo", 5, json(array, "bar", 10))', '^\["foo", 5, \["bar", 10\]\]$');

test('json.object.1', $mortal, 'think json(object, foo, 1, bar, "baz", boing, true)', '^{"foo": 1, "bar": "baz", "boing": true}$');
test('json.object.2', $mortal, 'think json(object, foo, 1, bar, "baz", boing, json(array, "nested", "test", 1)))', '^{"foo": 1, "bar": "baz", "boing": \["nested", "test", 1\]}');

test('json.patch.1', $mortal, "think json_mod(json(object,a,1,b,2),patch,json(object,c,3,d,4))", '^{"a":1,"b":2,"c":3,"d":4}');
test('json.patch.2', $mortal, "think json_mod(json(object,a,json(array,1,2),b,2), patch, json(object,a,9))", '^{"a":9,"b":2}');
test('json.patch.3', $mortal, 'think json_mod(json(object,a,json(array,1,2),b,2), patch, json(object, a, null))', '^{"b":2}');
test('json.patch.4', $mortal, 'think json_mod(json(object,a,1,b,2), patch, json(object,a,9,b,null,c,8))', '^{"a":9,"c":8}');
test('json.patch.5', $mortal, 'think json_mod(json(object,a,json(object,x,1,y,2),b,3), patch, json(object,a,json(object,y,9),c,8))', '^{"a":\{"x":1,"y":9\},"b":3,"c":8}');

