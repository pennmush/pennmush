run tests:
test('null.1', $god, 'think null()', '');
test('null.2', $god, 'think null(a)', '');
test('null.3', $god, 'think null(a,b,c)', '');
test('null.4', $god, 'think null(pemit(#1,test))', 'test');
test('null.5', $god, 'think @@()', '');
test('null.6', $god, 'think @@(a)', '');
test('null.7', $god, 'think @@(a,b,c)', '');
test('null.8', $god, 'think @@(pemit(#1,test))', '');
