run tests:

$god->command('&fun`chain`1 me=add(%0,1)');
$god->command('&fun`chain`2 me=mul(%0,2)');
test('chain.1', $god, 'think chain(lattr(me/fun`chain`),5)', '12');

$god->command('&fun`chain2`1 me=add(%0,1)[setq(0,5)]');
$god->command('&fun`chain2`2 me=sub(mul(%0,2),%q0)');
test('chain.2', $god, 'think chain(lattr(me/fun`chain2`),5)', '7');