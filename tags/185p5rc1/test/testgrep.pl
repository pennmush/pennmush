login mortal
run tests:
$mortal->command("&FIRST me=first");
$mortal->command("&SECOND me=second");
$mortal->command("&THIRD me=third");

test('grep.1', $mortal, 'think grep(me,*,d)', 'SECOND THIRD');
test('grep.2', $mortal, 'think grep(me,S*,d)', 'SECOND');
test('grep.3', $mortal, 'think grep(me,*,*d*)', '!SECOND THIRD');
test('grep.4', $mortal, 'think grep(me,*,D)', '!SECOND THIRD');

test('grep.5', $mortal, 'think wildgrep(me,*,*d*)', 'SECOND THIRD');
test('grep.6', $mortal, 'think wildgrep(me,*,d)', '!SECOND THIRD');
test('grep.7', $mortal, 'think wildgrep(me,*,first)', 'FIRST');
test('grep.8', $mortal, 'think wildgrep(me,*,FIRST)', '!FIRST');

test('grep.9', $mortal, 'think regrep(me,*,*d*)', '!SECOND THIRD');
test('grep.10', $mortal, 'think regrep(me,*,d)', 'SECOND THIRD');
test('grep.11', $mortal, 'think regrep(me,*,d$)', 'SECOND THIRD');
test('grep.12', $mortal, 'think regrep(me,*,first)', 'FIRST');
test('grep.13', $mortal, 'think regrep(me,*,FIRST)', '!FIRST');

test('grep.14', $mortal, 'think grepi(me,*,d)', 'SECOND THIRD');
test('grep.15', $mortal, 'think grepi(me,S*,d)', 'SECOND');
test('grep.16', $mortal, 'think grepi(me,*,*d*)', '!SECOND THIRD');
test('grep.17', $mortal, 'think grepi(me,*,D)', 'SECOND THIRD');

test('grep.18', $mortal, 'think wildgrepi(me,*,*d*)', 'SECOND THIRD');
test('grep.19', $mortal, 'think wildgrepi(me,*,d)', '!SECOND THIRD');
test('grep.20', $mortal, 'think wildgrepi(me,*,first)', 'FIRST');
test('grep.21', $mortal, 'think wildgrepi(me,*,FIRST)', 'FIRST');

test('grep.22', $mortal, 'think regrepi(me,*,*d*)', '!SECOND THIRD');
test('grep.23', $mortal, 'think regrepi(me,*,d)', 'SECOND THIRD');
test('grep.24', $mortal, 'think regrepi(me,*,d$)', 'SECOND THIRD');
test('grep.25', $mortal, 'think regrepi(me,*,first)', 'FIRST');
test('grep.26', $mortal, 'think regrepi(me,*,FIRST)', 'FIRST');
