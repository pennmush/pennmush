run tests:
test('soundex.1', $god, 'think soundex(a)', 'A000');
test('soundex.2', $god, 'think soundex(fred)', 'F630');
test('soundex.3', $god, 'think soundex(phred, soundex)', 'F630');
test('soundex.4', $god, 'think soundex(afford)', 'A163');

test('soundex.5', $god, 'think soundex(fred, phone)', '^BRD$');
test('soundex.6', $god, 'think soundex(phred, phone)', '^BRD$');
test('soundex.7', $god, 'think soundex(afford, phone)', '^ABRD$');

test('soundex.8', $god, 'think soundex(foo, bad hash)', '^#-1');

test('soundslike.1', $god, 'think soundslike(robin, robbyn)', '^1');
test('soundslike.2', $god, 'think soundslike(robin, roebuck)', '^0');
test('soundslike.3', $god, 'think soundslike(frick, frack)', '^1');
test('soundslike.4', $god, 'think soundslike(glacier, glazier)', '^1');
test('soundslike.5', $god, 'think soundslike(rutabega, rototiller, soundex)', '^0');

test('soundslike.6', $god, 'think soundslike(robin, robbyn, phone)', '^1');
test('soundslike.7', $god, 'think soundslike(robin, roebuck, phone)', '^0');
test('soundslike.8', $god, 'think soundslike(frick, frack, phone)', '^1');
test('soundslike.9', $god, 'think soundslike(glacier, glazier, phone)', '^1');
test('soundslike.10', $god, 'think soundslike(rutabega, rototiller, phone)', '^0');

test('soundslike.11', $god, 'think soundslike(foo, bar, bad hash)', '^#-1');

