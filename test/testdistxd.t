run tests:
test('dist2d.1', $god, 'think dist2d(0,0, 5,5)', "7.071068");
test('dist2d.2', $god, 'think lmath(dist2d, 0 0  5 5)', "7.071068");
test('dist2d.3', $god, 'think dist2d(0,0, 0,0)', "0");
test('dist2d.4', $god, 'think dist2d(0,0, 0,1)', "1");

test('dist3d.1', $god, 'think dist3d(0,0,0, 5,5,5)', "8\\.660254");
test('dist3d.2', $god, 'think lmath(dist3d, 0 0 0  5 5 5)', "8\\.660254"); 
test('dist3d.3', $god, 'think dist3d(0,0,0, 0,0,0)', "0");
test('dist3d.4', $god, 'think dist3d(0,0,0, 1,0,0)', "1");
