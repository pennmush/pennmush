run tests:

test('lnum.1', $god, 'think lnum()', '^#-1');
test('lnum.2', $god, 'think lnum(#1)', '^#-1');
test('lnum.3', $god, 'think lnum(foo)', '^#-1');
test('lnum.4', $god, 'think lnum(4.5)', '^0 1 2 3$');
test('lnum.5', $god, 'think lnum(5)', '^0 1 2 3 4$');
test('lnum.6', $god, 'think lnum(1,)', '^#-1');
test('lnum.7', $god, 'think lnum(,5)', '^#-1');
test('lnum.8', $god, 'think lnum(1,5)', '^1 2 3 4 5$');
test('lnum.9', $god, 'think lnum(1,4,@)', '^1@2@3@4$');
test('lnum.10', $god, 'think lnum(1,5,@,2)', '^1@3@5$');
test('lnum.11', $god, 'think lnum(1,5,,2)', '^135$');
test('lnum.12', $god, 'think lnum(-2,2)', '^-2 -1 0 1 2$');
test('lnum.13', $god, 'think lnum(1.5, 4.5)', '^1.5 2.5 3.5 4.5$');
test('lnum.14', $god, 'think lnum(1.5,4.5,%b,.5)', '^1.5 2 2.5 3 3.5 4 4.5$');
