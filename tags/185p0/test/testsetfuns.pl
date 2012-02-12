login mortal
run tests:
test('setunion.1', $mortal, 'think setunion(,)', '^$');
test('setunion.2', $mortal, 'think setunion(a,a)', '^a$');
test('setunion.3', $mortal, 'think setunion(c a b a,a b c c)', '^a b c$');
test('setunion.4', $mortal, 'think setunion(a a a,)', '^a$');
test('setunion.5', $mortal, 'think setunion(,a a a)', '^a$');

test('setinter.1', $mortal, 'think setinter(a,b)', '^$');
test('setinter.2', $mortal, 'think setinter(a b, a)', '^a$');

test('setdiff.1', $mortal, 'think setdiff(a, a)', '^$');
test('setdiff.2', $mortal, 'think setdiff(a, b)' , '^a$');
test('setdiff.3', $mortal, 'think setdiff(a b, b)', '^a$');
