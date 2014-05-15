run tests:
test('tr.1', $god, 'think tr(test STRING,,)', 'test STRING');
test('tr.2', $god, 'think tr(test STRING,t,)', '#-1');
test('tr.3', $god, 'think tr(test STRING,,t)', '#-1');
test('tr.4', $god, 'think tr(test STRING,t,f)', 'fesf STRING');
test('tr.5', $god, 'think tr(test STRING,tT,fF)', 'fesf SFRING');
test('tr.6', $god, 'think tr(test STRING,Tt,Ff)', 'fesf SFRING');
test('tr.7', $god, 'think tr(test STRING,te,et)', 'etse STRING');
