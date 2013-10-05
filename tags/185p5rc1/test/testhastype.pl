expect 2 failures!
run tests:
test('hastype.1', $god, 'think hastype(#0, room)', ['1', '!#-1']);
test('hastype.2', $god, 'think hastype(#1, player)', ['1', '!#-1']);
$god->command('@create foo');
test('hastype.3', $god, 'think hastype(foo, thing)', ['1', '!#-1']);
$god->command('@recycle foo');
$god->command('@recycle foo');
test('hastype.4', $god, 'think hastype(#3, garbage)', ['1', '!#-1']);
$god->command('@open foo');
test('hastype.5', $god, 'think hastype(#3, exit)', ['1', '!#-1']);

