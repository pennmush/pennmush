run tests:
$god->command('@config/set tiny_math=no');
test('ljust.1', $god, "think ljust(foo, 3)", 'foo');
test('ljust.2', $god, "think ljust(foo, 5)X", '^foo  X');
test('ljust.3', $god, "think ljust(foo, 5, =)", '^foo==');
test('ljust.4', $god, "think ljust(foo, 2)", '^foo');
test('ljust.5', $god, "think ljust(foo, -3)", '^#-1 ARGUMENT MUST BE POSITIVE INTEGER');

test('rjust.1', $god, "think rjust(foo, 3)", 'foo');
test('rjust.2', $god, "think rjust(foo, 5)", '^  foo');
test('rjust.3', $god, "think rjust(foo, 5, =)", '^==foo');
test('rjust.4', $god, "think rjust(foo, 2)", '^foo');
test('rjust.5', $god, "think rjust(foo, -3)", '^#-1 ARGUMENT MUST BE POSITIVE INTEGER');

test('center.1', $god, "think center(foo, 3)", 'foo');
test('center.2', $god, "think #[center(fo, 3)]#", '^#fo #');
test('center.3', $god, "think center(foo, 5)X", '^ foo X');
test('center.4', $god, "think center(foo, 5, =)", '^=foo=');
test('center.5', $god, "think center(foo, 2)", '^foo');
test('center.6', $god, "think center(foo, -3)", '^#-1 ARGUMENT MUST BE POSITIVE INTEGER');
