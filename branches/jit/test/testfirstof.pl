run tests:
test('firstof.1', $god, 'think firstof(0,0,2)', '2');
test('firstof.2', $god, 'think firstof(2,0,0)', '2');
test('firstof.3', $god, 'think firstof(0,0,0)', '0');
test('firstof.4', $god, 'think firstof(1,2,3)', '1');
test('firstof.5', $god, 'think allof(0,0,2,)', '2');
test('firstof.6', $god, 'think allof(2,0,0,)', '2');
test('firstof.7', $god, 'think allof(0,0,0,)', '');
test('firstof.8', $god, 'think allof(1,2,3,%b)', '1 2 3');
test('firstof.9', $god, 'think allof(1,2,3,)', '123');
test('firstof.9', $god, 'think allof(0,0,2,|)', '2');
test('firstof.10', $god, 'think allof(2,0,0,|)', '2');
test('firstof.11', $god, 'think allof(0,0,0,|)', '');
test('firstof.12', $god, 'think allof(1,2,3,|)', '1|2|3');
