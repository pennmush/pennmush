login mortal
run tests:
test('setunion.1', $mortal, 'think setunion(,)', '^$');
test('setunion.2', $mortal, 'think setunion( a,a)', '^a\r$');
test('setunion.3', $mortal, 'think setunion(c a b a,a b c c)', '^a b c\r$');
test('setunion.4', $mortal, 'think setunion(a a a,)', '^a\r$');
test('setunion.5', $mortal, 'think setunion(,a a a)', '^a\r$');
