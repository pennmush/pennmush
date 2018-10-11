run tests:
# Sort
# sort.1 returns inconsistent results. Disabled for now.
#test('sort.1', $god, 'think sort(0.0 0 0.3 *foo*)', '^\*foo\*');
test('sort.2', $god, 'think sort(0.0 0 0.3 *foo*,f)', '0 \*foo\* 0.3');
test('sort.3', $god, 'think sort(a [ansi(h,a)] b [ansi(h,b)] c d [ansi(h,e)] f)', 'a a b b c d e f');
test('sort.4', $god, 'think sort(3 [ansi(h,1)] [ansi(y,7)] 5)', '1 3 5 7');
