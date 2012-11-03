run tests:
test('soundex.1', $god, 'think soundex(a)', 'A000');
test('soundex.2', $god, 'think soundex(0)', '#-1 FUNCTION \(SOUNDEX\) REQUIRES A SINGLE WORD ARGUMENT');
test('soundex.3', $god, 'think soundex(fred)', 'F630');
test('soundex.4', $god, 'think soundex(phred)', 'F630');
test('soundex.5', $god, 'think soundex(afford)', 'A163');

test('soundslike.1', $god, 'think soundslike(robin, robbyn)', '1');
test('soundslike.2', $god, 'think soundslike(robin, roebuck)', '0');
test('soundslike.3', $god, 'think soundslike(frick, frack)', 1);
test('soundslike.4', $god, 'think soundslike(glacier, glazier)', 1);
test('soundslike.5', $god, 'think soundslike(rutabega, rototiller)', 0);

