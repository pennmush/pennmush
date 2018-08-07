login mortal
run tests:

$mortal->command('&p.1 me=[add(1,2)]');
$mortal->command('&p.2 me=[add(1,2))]');
$mortal->command('&p.3 me={[()]}');
$mortal->command('@set me=!ansi');

# Colors are a pain to test, so mostly just check formatting options and basic sanity.

test('parenmatch.1', $mortal, 'think parenmatch(me/p.1)', '^\[add\(1,2\)\]$');
test('parenmatch.2', $mortal, 'think parenmatch(p.1, 1)', '^\[add\(1,2\)\]$');
test('parenmatch.3', $mortal, 'think parenmatch(me/p.1, 0, 1)', '(?m)^\[\r\n\s+add\(\r\n\s+1,2\r\n\s+\)\r\n\]$');

test('parenmatch.4', $mortal, 'think parenmatch(me/p.2)', '^\[add\(1,2\)\)\]$');
test('parenmatch.5', $mortal, 'think parenmatch(p.2, 1)', '^\[add\(1,2\)\)\]$');
test('parenmatch.6', $mortal, 'think parenmatch(me/p.2, 0, 1)', '(?m)^\[\r\n\s+add\(\r\n\s+1,2\r\n\s+\)\)\]$');

test('parenmatch.7', $mortal, 'think parenmatch(me/p.1, 0, 0, s)', '^\[add\(1, 2\)\]$');
test('parenmatch.8', $mortal, 'think parenmatch(me/p.1, 0, 0, p)', '^\[add\( 1,2 \)\]$');
test('parenmatch.9', $mortal, 'think parenmatch(me/p.1, 0, 0, b)', '^\[ add\(1,2\) \]$');
test('parenmatch.10', $mortal, 'think parenmatch(me/p.1, 0, 0, spb)', '^\[ add\( 1, 2 \) \]$');
test('parenmatch.11', $mortal, 'think parenmatch(me/p.3, 0, 0, B)', '^\{ \[\(\)\] \}$');

$mortal->command('@set me=ansi color');
test('parenmatch.12', $mortal, 'think parenmatch(p.2, 1)', '^\[add\(1,2\)\x1B\[1;31m\)\x1B\[0m\]$');
$mortal->command('@set me=!ansi !color');
