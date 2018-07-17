login mortal
run tests:

# Time functions.

# etimefmt
test('etimefmt.1', $mortal, 'think etimefmt($w $d $h $m $s, 59)', '^0 0 0 0 59$');
test('etimefmt.2', $mortal, 'think etimefmt($w $d $h $m $s, 75)', '^0 0 0 1 15$');
test('etimefmt.3', $mortal, 'think etimefmt($w $d $h $m $s, 3665)', '^0 0 1 1 5$');
test('etimefmt.4', $mortal, 'think etimefmt($w $d $h $m $s, add(mul(60, 60, 24, 8), 50))', '^1 1 0 0 50$');
test('etimefmt.5', $mortal, 'think etimefmt($d $h $m $s, add(mul(60, 60, 24, 8), 50))', '^8 0 0 50$');
test('etimefmt.6', $mortal, 'think etimefmt($ts, 75)', '75');
test('etimefmt.7', $mortal, 'think etimefmt($xm$xs, 75)', '1m15s');
test('etimefmt.8', $mortal, 'think etimefmt(test $2S, 5)', '^test 05$');
test('etimefmt.9', $mortal, 'think etimefmt($2s test, 5)', '^ 5 test$');
test('etimefmt.10', $mortal, 'think etimefmt($zm $zs, 45)', '^ 45$');
test('etimefmt.11', $mortal, 'think etimefmt($zxm $zxs, 45)', '^ 45s$');
test('etimefmt.12', $mortal, 'think etimefmt($xzd $xth, 86405)', '^1d 24h$');
test('etimefmt.13', $mortal, 'think etimefmt($h, -200)', '#-1');
test('etimefmt.14', $mortal, 'think etimefmt($h, twelve)', '#-1');
test('etimefmt.15', $mortal, 'think etimefmt($2h:$2M, 3700)', '1:01');
test('etimefmt.16', $mortal, 'think etimefmt(You have $m minutes and $s seconds to go, 78)', 'You have 1 minutes and 18 seconds to go');
test('etimefmt.17', $mortal, 'think etimefmt($y $d, mul(60, 60, 24, 367))', '^1 2$');
test('etimefmt.18', $mortal, 'think etimefmt($y $td, mul(60, 60, 24, 367))', '^1 367$');

# timestring
test('timestring.1', $mortal, 'think timestring(301)', '^ 5m  1s$');
test('timestring.2', $mortal, 'think timestring(301, 1)', '^0d  0h  5m  1s$');
test('timestring.3', $mortal, 'think timestring(301, 2)', '^00d 00h 05m 01s$');
test('timestring.4', $mortal, 'think timestring(-50)', '#-1');
test('timestring.5', $mortal, 'think timestring(four)', '#-1');

# stringsecs
test('stringsecs.1', $mortal, 'think stringsecs(5m 1s)', '301');
test('stringsecs.2', $mortal, 'think stringsecs(3y 2m 7d 5h 23m)', '95232300');
test('stringsecs.3', $mortal, 'think stringsecs(foo)', '#-1');

# etime
test('etime.1', $mortal, 'think etime(59)', '^59s$');
test('etime.2', $mortal, 'think etime(60)', '^1m$');
test('etime.3', $mortal, 'think etime(61)', '^1m  1s$');
test('etime.4', $mortal, 'think etime(61, 5)', '^1m$');
test('etime.5', $mortal, 'think etime(foo)', '#-1');
test('etime.6', $mortal, 'think etime(50, foo)', '#-1');
