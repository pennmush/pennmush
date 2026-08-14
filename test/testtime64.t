login mortal
run tests:
# Y2038: time functions must accept epoch values past 2^31-1 (Jan 19 2038).
# 4102444800 is Fri Jan  1 00:00:00 2100 UTC.
test('time64.convsecs.1', $mortal, 'think convutcsecs(4102444800)', '^Fri Jan 01 00:00:00 2100$');
test('time64.convsecs.2', $mortal, 'think convsecs(2147483648, utc)', '^Tue Jan 19 03:14:08 2038$');
test('time64.convsecs.3', $mortal, 'think convsecs(psychic-friends-network)', 'ARGUMENT MUST BE INTEGER');
# convtime already returned 64-bit values; the round trip must work both ways.
test('time64.roundtrip.1', $mortal, 'think convutctime(convutcsecs(4102444800))', '^4102444800$');
test('time64.timefmt.1', $mortal, 'think timefmt($Y-$m-$d $H:$M:$S, 4102444800, utc)', '^2100-01-01 00:00:00$');
# isdaylight with a post-2038 epoch should answer, not reject the argument.
test('time64.isdaylight.1', $mortal, 'think isdaylight(4102444800)', '^[01]$');
# objid parsing must accept 64-bit creation timestamps.
test('time64.objid.1', $mortal, 'think num(#1:[csecs(#1)])', '^#1$');
test('time64.objid.2', $mortal, 'think num(#1:4102444800)', '#-1$');
# @wait/until with a post-2038 absolute time: over 2 billion seconds remain.
# Halted pids linger in lpids() until the next queue cycle, so always
# check the newest entry via last().
$mortal->command('@wait/until 4102444800=think this should not run for decades');
test('time64.waituntil.1', $mortal, 'think gt(pidinfo(last(lpids()), time), 2000000000)', '^1$');
$mortal->command('@halt/pid [last(lpids())]');
# Semaphore form: @wait/until <object>/<64-bit time>=command
$mortal->command('@wait/until me/4102444800=think this should not run for decades');
test('time64.semwait.1', $mortal, 'think gt(pidinfo(last(lpids()), time), 2000000000)', '^1$');
$mortal->command('@drain me');
# A huge relative wait must saturate, not overflow into the past and
# run immediately.
$mortal->command('@wait 9223372036854775807=think this should not run for decades');
test('time64.relwait.1', $mortal, 'think gt(pidinfo(last(lpids()), time), 2000000000)', '^1$');
$mortal->command('@halt/pid [last(lpids())]');
