run tests:

$god->command('&fun`arrow`1 me=add(%0,1)');
$god->command('&fun`arrow`2 me=mul(%0,2)');
test('arrow.1', $god, 'think arrow(lattr(me/fun`arrow`),5)', '12');

$god->command('&fun`arrow2`1 me=add(%0,1)[setq(0,5)]');
$god->command('&fun`arrow2`2 me=sub(mul(%0,2),%q0)');
test('arrow.2', $god, 'think arrow(lattr(me/fun`arrow2`),5)', '7');