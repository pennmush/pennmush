run tests:
test('insert.1', $god, 'think insert(a b c,0,X)', 'a b c');
test('insert.2', $god, 'think insert(a b c,1,X)', 'X a b c');
test('insert.3', $god, 'think insert(a b c,2,X)', 'a X b c');
test('insert.4', $god, 'think insert(a b c,3,X)', 'a b X c');
test('insert.5', $god, 'think insert(a b c,4,X)', 'a b c');
test('insert.6', $god, 'think insert(a b c,-1,X)', 'a b c X');
test('insert.7', $god, 'think insert(a b c,-2,X)', 'a b X c');
test('insert.8', $god, 'think insert(a b c,-3,X)', 'a X b c');
test('insert.9', $god, 'think insert(a b c,-4,X)', 'a b c');
test('insert.10', $god, 'think insert(a|b|c,0,|)', 'a|b|c');
test('insert.11', $god, 'think insert(a|b|c,1,|)', 'X|a|b|c');
test('insert.12', $god, 'think insert(a|b|c,-1,|)', 'a|b|c|X');
test('insert.13', $god, 'think insert(a|b|c,2,|)', 'a|X|b|c');
test('insert.14', $god, 'think insert(a|b|c,-2,|)', 'a|b|X|c');
