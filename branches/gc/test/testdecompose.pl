login mortal
run tests:
test('decompose.1', $mortal, 'think decompose([ansi(hr,b[ansi(f,la)]h)])', '\[ansi\(hr,b(\)\])?\[ansi\((?(1)fhr|f),la\)\](?(1)\[ansi\(hr,h|h)\)\]');
test('decompose.2', $mortal, 'think decompose(a\ \ \ \ b)', 'a %b %bb');
test('decompose.3', $mortal, 'think decompose(s(tab%treturn%r))', 'tab%treturn%r');
test('decompose.4', $mortal, 'think decompose(before(ansi(h,x),x)hello)', 'hello');
test('decompose.5', $mortal, 'think decompose([before(ansi(h,blah),\[)])', '\[ansi\(h,blah\)\]');
