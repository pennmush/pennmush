run tests:

test('hasflag.1', $god, 'think hasflag(me, wizard)', '^1$');
test('hasflag.2', $god, 'think hasflag(me, flunky)', '^0$');
test('hasflag.3', $god, 'think hasflag(me, puppet)', '^0$');

test('andlflags.1', $god, 'think andlflags(me, wizard connected)', '^1$');
test('andlflags.2', $god, 'think andlflags(me, wizard flunky)', '^0$');
test('andlflags.3', $god, 'think andlflags(me, wizard !noaccents)', '^1$');
test('andlflags.4', $god, 'think andlflags(me, wizard !puppet)', '^1$');
test('andlflags.5', $god, 'think andlflags(me, puppet wizard)', '^0$');
test('andlflags.6', $god, 'think andlflags(me, noaccents wizard)', '^0$');
test('andlflags.7', $god, 'think andlflags(#1234, myopic)', "I can't see that here.");
test('andlflags.8', $god, 'think andlflags(me, player connected)', '^1$');
test('andlflags.9', $god, 'think andlflags(me, connected ! myopic)', '^#-1');

test('andflags.1', $god, 'think andflags(me, Wc)', '^1$');
test('andflags.2', $god, 'think andflags(me, W_)', '^0$');
test('andflags.3', $god, 'think andflags(me, W~)', '^0$');
test('andflags.4', $god, 'think andflags(me, W!~)', '^1$');
test('andflags.5', $god, 'think andflags(me, W!)', '^#-1');
test('andflags.6', $god, 'think andflags(me, WP)', '^1$');
test('andflags.7', $god, 'think andflags(me, WT)', '^0$');

test('orlflags.1', $god, 'think orlflags(me, wizard connected)', '^1$');
test('orlflags.2', $god, 'think orlflags(me, wizard flunky)', '^1$');
test('orlflags.3', $god, 'think orlflags(me, flunky wizard)', '^1$');
test('orlflags.4', $god, 'think orlflags(me, myopic noaccents)', '^0$');
test('orlflags.5', $god, 'think orlflags(me, myopic !noaccents)', '^1$');
test('orlflags.6', $god, 'think orlflags(#1234, myopic)', "I can't see that here.");
test('orlflags.7', $god, 'think orlflags(me, thing player)', '^1$');
test('orlflags.8', $god, 'think orlflags(me, noaccents ! myopic)', '^#-1');

test('orflags.1', $god, 'think orflags(me, ~W)', '^1$');
test('orflags.2', $god, 'think orflags(me, ~_)', '^0$');
test('orflags.3', $god, 'think orflags(me, v!~)', '^1$');
test('orflags.4', $god, 'think orflags(me, v!)', '^#-1');
test('orflags.5', $god, 'think orflags(me, ET)', '^0$');
test('orflags.6', $god, 'think orflags(me, EP)', '^1$');

