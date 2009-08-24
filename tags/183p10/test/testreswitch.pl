run tests:
test('reswitch.1', $god, 'think reswitch(test STRING,t,1,0)', '1');
test('reswitch.2', $god, 'think reswitch(test STRING,t,1,e,2,0)', '1');
test('reswitch.3', $god, 'think reswitch(test STRING,E,1,0)', '0');
test('reswitch.4', $god, 'think reswitch(test STRING,.\{4\}\\\s\[A-Z\]\{6\},9,t,1,E,2,0)', '9');

test('reswitch.5', $god, 'think reswitchall(test STRING,t,1,0)', '1');
test('reswitch.6', $god, 'think reswitchall(test STRING,t,1,e,2,0)', '12');
test('reswitch.7', $god, 'think reswitchall(test STRING,E,1,0)', '0');
test('reswitch.8', $god, 'think reswitchall(test STRING,.\{4\}\\\s\[A-Z\]\{6\},9,t,1,E,2,0)', '91');

test('reswitch.9', $god, 'think reswitchi(test STRING,t,1,0)', '1');
test('reswitch.10', $god, 'think reswitchi(test STRING,t,1,e,2,0)', '1');
test('reswitch.11', $god, 'think reswitchi(test STRING,E,1,0)', '1');
test('reswitch.12', $god, 'think reswitchi(test STRING,.\{4\}\\\s\[A-Z\]\{6\},9,t,1,E,2,0)', '9');

test('reswitch.13', $god, 'think reswitchalli(test STRING,t,1,0)', '1');
test('reswitch.14', $god, 'think reswitchalli(test STRING,t,1,e,2,0)', '12');
test('reswitch.15', $god, 'think reswitchalli(test STRING,E,1,0)', '1');
test('reswitch.16', $god, 'think reswitchalli(test STRING,.\{4\}\\\s\[A-Z\]\{6\},9,t,1,E,2,0)', '912');
