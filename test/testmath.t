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

test('cos.1',$god, 'think cos(90,d)', '^-?0$');
test('cos.2',$god, 'think cos(pi(),r)', '^-1$');
test('cos.3',$god, 'think cos(pi())', '^-1$');

test('acos.1',$god, 'think acos(cos(90,d),d)', '^90$');
test('acos.2',$god, 'think acos(cos(1,r))', '^1$');
test('acos.3',$god, 'think acos(cos(1,r),r)', '^1$');

test('sin.1',$god, 'think sin(90,d)', '^1$');
test('sin.2',$god, 'think sin(pi(),r)', '^-?0$');
test('sin.3',$god, 'think sin(pi())', '^-?0$');

test('asin.1',$god, 'think asin(sin(90,d),d)', '^90$');
test('asin.2',$god, 'think asin(sin(1,r))', '^1$');
test('asin.3',$god, 'think asin(sin(1,r),r)', '^1$');

test('tan.1',$god, 'think tan(45,d)', '^1$');
test('tan.2',$god, 'think tan(pi(),r)', '^-?0$');
test('tan.3',$god, 'think tan(pi())', '^-?0$');
test('tan.4', $god, 'think tan(90, d)', '^#-1');

test('atan.1',$god, 'think atan(tan(45,d),d)', '^45$');
test('atan.2',$god, 'think atan(tan(1,r))', '^1$');
test('atan.3',$god, 'think atan(tan(1,r),r)', '^1$');

test('atan2.1', $god, 'think atan2(0, -1)', '^3.141593$');
test('atan2.2', $god, 'think atan2(0, 1)', '^0$');
test('atan2.3', $god, 'think atan2(-0.0001, 0)', '^-1.570796$');
test('atan2.4', $god, 'think atan2(0.0001, 0)', '^1.570796$');

test('ctu.1',$god, 'think ctu(90,d,r)', '^1.570\d*$');
test('ctu.2',$god, 'think ctu(pi(),r,d)','^180(\.00\d*)?$');

test('sqrt.1', $god, 'think sqrt(4)', '2');
test('sqrt.2', $god, 'think sqrt(-1)', '#-1 IMAGINARY NUMBER');

test('root.1', $god, 'think root(4,2)', '2');
test('root.2', $god, 'think root(-1,2)', '#-1 IMAGINARY NUMBER');
test('root.3', $god, 'think root(27, 3)', '3');
test('root.4', $god, 'think root(-27, 3)', '-3');
$god->command('@config/set float_precision=10');
test('root.5', $god, 'think root(125, 5)', '2.6265278044');
$god->command('@config/set float_precision=6');

test('round.0', $god, 'think round(pi(), 0)', '3');
test('round.1', $god, 'think round(pi(), 1)', '3.1');
test('round.2', $god, 'think round(pi(), 2)', '3.14$');
test('round.3', $god, 'think round(pi(), 3)', '3.142$');
test('round.4', $god, 'think round(pi(), 4)', '3.1416$');
test('round.5', $god, 'think round(pi(), 5)', '3.14159$');
test('round.6', $god, 'think round(-[pi()], 3)', '-3.142$');
test('round.7', $god, 'think round(3.5, 3, 1)', '3.500$');
test('round.8', $god, 'think round(1.2345, 2, 1)', '1.23$');

test('div.1', $god, 'think div(13,4)', '^3');
test('div.2', $god, 'think div(-13,4)', '^-3');
test('div.3', $god, 'think div(13,-4)', '^-3');
test('div.4', $god, 'think div(-13,-4)', '^3');

test('floordiv.1', $god, 'think floordiv(13,4)', '^3');
test('floordiv.2', $god, 'think floordiv(-13,4)', '^-4');
test('floordiv.3', $god, 'think floordiv(13,-4)', '^-4');
test('floordiv.4', $god, 'think floordiv(-13,-4)', '^3');

test('mod.1', $god, 'think modulo(13,4)', '^1');
test('mod.2', $god, 'think modulo(-13,4)', '^3');
test('mod.3', $god, 'think modulo(13,-4)', '^-3');
test('mod.4', $god, 'think modulo(-13,-4)', '^-1');

test('remainder.1', $god, 'think remainder(13,4)', '^1');
test('remainder.2', $god, 'think remainder(-13,4)', '^-1');
test('remainder.3', $god, 'think remainder(13,-4)', '^1');
test('remainder.4', $god, 'think remainder(-13,-4)', '^-1');

test('sign.1', $god, 'think sign(-4)', '^-1$');
test('sign.2', $god, 'think sign(4)', '^1$');
test('sign.3', $god, 'think sign(0)', '^0$');

test('mean.1', $god, 'think mean(1,2,3,4,5)', '^3$');
test('median.1', $god, 'think median(1,2,3,4,5)', '^3$');
test('median.2', $god, 'think median(1,2,3,4)', '^2.5$');
test('stddev.1', $god, 'think stddev(1,2,3,4,5)', '^1.581139$');

test('log.1', $god, 'think log(0)', '^-inf');
test('log.2', $god, 'think log(1)', '^0$');
test('log.3', $god, 'think log(100)', '^2$');
test('log.4', $god, 'think log(8,2)', '^3$');
test('log.5', $god, 'think log(10,e)', '^2.302585$');
test('log.6', $god, 'think ln(10)', '^2.302585$');
test('log.7', $god, 'think log(9,3)', '^2$');
test('log.8', $god, 'think log(9,foo)', '^#-1');
test('log.9', $god, 'think log(-5)', '^#-1');

test('fraction.1', $god, 'think fraction(.75)', '^3/4$');
test('fraction.2', $god, 'think fraction(pi())', '^348987/111086$');
test('fraction.3', $god, 'think fraction(2)', '^2$');
test('fraction.4', $god, 'think fraction(2.75)', '^11/4$');
test('fraction.5', $god, 'think fraction(2.75, 1)', '^2 3/4$');
test('fraction.6', $god, 'think fraction(2, 1)', '^2$');

test('inc.1', $god, 'think inc(0)', '^1$');
test('inc.2', $god, 'think inc(-2)', '^-1$');
test('inc.3', $god, 'think inc(foo1)', '^foo2$');
test('inc.4', $god, 'think inc(1.2)', '^1.3$');
test('inc.5', $god, 'think inc(foo)', '^#-1');

test('dec.1', $god, 'think dec(0)', '^-1$');
test('dec.2', $god, 'think dec(-2)', '^-3$');
test('dec.3', $god, 'think dec(foo1)', '^foo0$');
test('dec.4', $god, 'think dec(1.2)', '^1.1$');
test('dec.5', $god, 'think dec(foo)', '^#-1');
