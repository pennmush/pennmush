run tests:
test('strreplace.1', $god, 'think strreplace(0010, 2, 1, 0)', '^0000$');
test('strreplace.2', $god, 'think strreplace(0010, 2, 5, 011)', '^00011$');
test('strreplace.3', $god, 'think strreplace(0010, 2, 2, 0)', '^000$');
test('strreplace.4', $god, 'think strreplace(0010, 2, 1, 010)', '^000100$');
test('strreplace.5', $god, 'think strreplace(0010, 6, 1, 0)', '^0010$');
test('strreplace.6', $god, 'think strreplace(0010, -1, 4, woot)', '^#-1');
test('strreplace.7', $god, 'think strreplace(AA[ansi(r, BB)]A, 2, 2, C)', '^AACA$');

test('strinsert.1', $god, 'think strinsert(000, 1, 1)', '^0100$');
test('strinsert.2', $god, 'think strinsert(000, -1, 1)', '^#-1');
test('strinsert.3', $god, 'think strinsert(000, 5, 11)', '^00011$');


