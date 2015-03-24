run tests:
test('hastype.1', $god, 'think hastype(#0, room)', ['1', '!#-1']);
test('hastype.2', $god, 'think hastype(#1, player)', ['1', '!#-1']);
my ($foo_dbref) = $god->command('@create foo') =~ m/ (\#\d+)\./;
test('hastype.3', $god, "think hastype($foo_dbref, thing)", ['1', '!#-1']);
$god->command("\@recycle $foo_dbref");
$god->command("\@recycle $foo_dbref");
test('hastype.4', $god, "think hastype($foo_dbref, garbage)", ['1', '!#-1']);
$god->command('@open foo');
test('hastype.5', $god, "think hastype($foo_dbref, exit)", ['1', '!#-1']);
