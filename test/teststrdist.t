login mortal
run tests:
test('strdistance.1', $mortal, 'think strdistance(this, thiz)', '1');
test('strdistance.2', $mortal, 'think strdistance(this, This)', '1');
test('strdistance.3', $mortal, 'think strdistance(this, This, 1)', '0');
test('strdistance.4', $mortal, 'think strdistance(kitten, sitting)', '3');
test('strdistance.5', $mortal, 'think strdistance(Saturday, sunday)', '4');
test('strdistance.6', $mortal, 'think strdistance(Saturday, sunday, 1)', '3');
test('strdistance.7', $mortal, 'think strdistance(foo,)', '-1');
test('strdistance.8', $mortal, 'think strdistance(,bar)', '-1');
