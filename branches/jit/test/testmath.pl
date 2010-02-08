run tests:
test('abs.1', $god, 'think abs(-1)', '1');
test('abs.2', $god, 'think abs(-1.5)', '1.5');
test('abs.3', $god, 'think abs(1)', '1');
test('abs.4', $god, 'think abs(0)', '0');
test('abs.5', $god, 'think abs(-0)', '0');
test('abs.6', $god, 'think abs(99999999999)', '99999999999');
test('abs.7', $god, 'think abs(-99999999999)', '99999999999');

test('baseconv.1', $god, 'think baseconv(10,10,36)', 'a');
test('baseconv.2', $god, 'think baseconv(-10,10,36)', '-a');
test('baseconv.3', $god, 'think baseconv(9,36,10)', '9');
test('baseconv.4', $god, 'think baseconv(-9,36,10)', '-9');
test('baseconv.5', $god, 'think baseconv(abc,36,10)', '13368');
test('baseconv.6', $god, 'think baseconv(-abc,36,10)', '-13368');
test('baseconv.7', $god, 'think baseconv(13368,10,36)', 'abc');
test('baseconv.8', $god, 'think baseconv(-13368,10,36)', '-abc');
test('baseconv.9', $god, 'think baseconv(100,10,64)', 'Bk');
test('baseconv.10', $god, 'think baseconv(Bk,64,10)', '100');
test('baseconv.11', $god, 'think baseconv(-Bk,64,10)', '254052');
test('baseconv.12', $god, 'think baseconv(-_,64,10)', '4031');
test('baseconv.13', $god, 'think baseconv(+/,64,10)', '4031');
test('baseconv.14', $god, 'think baseconv(4031,10,64)', '-_');

test('cos.1',$god, 'think cos(90,d)', '^-?0\s*$');
test('cos.2',$god, 'think cos(pi(),r)', '^-1\s*$');
test('cos.3',$god, 'think cos(pi())', '^-1\s*$');

test('acos.1',$god, 'think acos(cos(90,d),d)', '^90\s*$');
test('acos.2',$god, 'think acos(cos(1,r))', '^1\s*$');
test('acos.3',$god, 'think acos(cos(1,r),r)', '^1\s*$');

test('sin.1',$god, 'think sin(90,d)', '^1\s*$');
test('sin.2',$god, 'think sin(pi(),r)', '^-?0\s*$');
test('sin.3',$god, 'think sin(pi())', '^-?0\s*$');

test('asin.1',$god, 'think asin(sin(90,d),d)', '^90\s*$');
test('asin.2',$god, 'think asin(sin(1,r))', '^1\s*$');
test('asin.3',$god, 'think asin(sin(1,r),r)', '^1\s*$');

test('tan.1',$god, 'think tan(45,d)', '^1\s*$');
test('tan.2',$god, 'think tan(pi(),r)', '^-?0\s*$');
test('tan.3',$god, 'think tan(pi())', '^-?0\s*$');

test('atan.1',$god, 'think atan(tan(45,d),d)', '^45\s*$');
test('atan.2',$god, 'think atan(tan(1,r))', '^1\s*$');
test('atan.3',$god, 'think atan(tan(1,r),r)', '^1\s*$');

test('ctu.1',$god, 'think ctu(90,d,r)', '^1.570\d*\s*$');
test('ctu.2',$god, 'think ctu(pi(),r,d)','^180(\.00\d*)?\s*$');

test('sqrt.1', $god, 'think sqrt(4)', '2');
test('sqrt.2', $god, 'think sqrt(-1)', '#-1 IMAGINARY NUMBER');

test('root.1', $god, 'think root(4,2)', '2');
test('root.2', $god, 'think root(-1,2)', '#-1 IMAGINARY NUMBER');
test('root.3', $god, 'think root(27, 3)', '3');
test('root.4', $god, 'think root(-27, 3)', '-3');
$god->command('@config/set float_precision=10');
test('root.5', $god, 'think root(125, 5)', '2.6265278044');

test('round.0', $god, 'think round(pi(), 0)', '3');
test('round.1', $god, 'think round(pi(), 1)', '3.1');
test('round.2', $god, 'think round(pi(), 2)', '3.14');
test('round.3', $god, 'think round(pi(), 3)', '3.142');
test('round.4', $god, 'think round(pi(), 4)', '3.1416');
test('round.5', $god, 'think round(pi(), 5)', '3.14159');
test('round.6', $god, 'think round(-[pi()], 3)', '-3.142');
