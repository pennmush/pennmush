run tests:
test('firstof.1', $god, 'think firstof(0,0,2)', '^2$');
test('firstof.2', $god, 'think firstof(2,0,0)', '^2$');
test('firstof.3', $god, 'think firstof(0,0,0)', '^0$');
test('firstof.4', $god, 'think firstof(1,2,3)', '^1$');

test('strfirstof.1', $god, 'think strfirstof(,,foo)', '^foo$');
test('strfirstof.2', $god, 'think strfirstof(,bar,foo)', '^bar$');
test('strfirstof.3', $god, 'think strfirstof(bar,,foo)', '^bar$');
test('strfirstof.4', $god, 'think strfirstof(bar,baz,foo)', '^bar$');

test('allof.1', $god, 'think allof(0,0,2,)', '^2$');
test('allof.2', $god, 'think allof(2,0,0,)', '^2$');
test('allof.3', $god, 'think allof(0,0,0,)', '^$');
test('allof.4', $god, 'think allof(1,2,3,%b)', '^1 2 3$');
test('allof.5', $god, 'think allof(1,2,3,)', '^123$');
test('allof.6', $god, 'think allof(0,0,2,@)', '^2$');
test('allof.7', $god, 'think allof(2,0,0,@)', '^2$');
test('allof.8', $god, 'think allof(0,0,0,@)', '^$');
test('allof.9', $god, 'think allof(1,2,3,@)', '^1@2@3$');

test('strallof.1', $god, 'think strallof(,,,@)', '^$');
test('strallof.2', $god, 'think strallof(foo,@)', '^foo$');
test('strallof.3', $god, 'think strallof(,foo,@)', '^foo$');
test('strallof.4', $god, 'think strallof(foo,,@)', '^foo$');
test('strallof.5', $god, 'think strallof(foo,bar,@)', '^foo@bar$');
test('strallof.6', $god, 'think strallof(,foo,bar,@)', '^foo@bar$');
test('strallof.7', $god, 'think strallof(foo,,bar,@)', '^foo@bar$');
test('strallof.8', $god, 'think strallof(foo,bar,,@)', '^foo@bar$');

