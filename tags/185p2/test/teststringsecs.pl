login mortal
run tests:
test('stringsecs.1', $mortal, 'think stringsecs(a)', '#-1 INVALID TIMESTRING');
test('stringsecs.2', $mortal, 'think stringsecs(10)', '10');
test('stringsecs.3', $mortal, 'think stringsecs(10s)', '10');
test('stringsecs.4', $mortal, 'think stringsecs(5m)', '300');
test('stringsecs.5', $mortal, 'think stringsecs(5m 10s)', '310');
test('stringsecs.6', $mortal, 'think stringsecs(1h)', '3600');
test('stringsecs.7', $mortal, 'think stringsecs(10s 5m)', '310');
test('stringsecs.8', $mortal, 'think stringsecs(1d 2h 3m 4s)', '93784');
test('stringsecs.9', $mortal
     , 'think stringsecs(h)', '#-1 INVALID TIMESTRING');
