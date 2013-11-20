run tests:
test('trimpenn.1', $god, 'think trimpenn(XXXfooXXX, X, l)', 'fooXXX');
test('trimpenn.2', $god, 'think trimpenn(XXXfooXXX, X, r)', 'XXXfoo');
test('trimpenn.3', $god, 'think trimpenn(XXXfooXXX, X, b)', 'foo');
test('trimpenn.4', $god, 'think trimpenn(XXXfooXXX, Y, l)', 'XXXfooXXX');

test('trimtiny.1', $god, 'think trimtiny(XXXfooXXX, L, X)', 'fooXXX');
test('trimtiny.2', $god, 'think trimtiny(XXXfooXXX, R, X)', 'XXXfoo');
test('trimtiny.3', $god, 'think trimtiny(XXXfooXXX, B, X)', 'foo');
test('trimtiny.4', $god, 'think trimtiny(XXXfooXXX, l, Y)', 'XXXfooXXX');

$god->command('@config/set tiny_trim_fun=yes');
test('trim.1', $god, 'think trim(XXXfooXXX, l, X)', 'fooXXX');
$god->command('@config/set tiny_trim_fun=no');
test('trim.2', $god, 'think trim(XXXfooXXX, X, l)', 'fooXXX');
test('trim.3', $god, 'think @[trim(%b%bfoo%b%b)]@', 'foo');


