login mortal
run tests:
# First, the basic tests enforcing tree-nature of the attributes.
# Attrs may not start or end in `
test("atree.basic.1", $god, "&foo` me=baz", "not a very good name");
test("atree.basic.2", $god, "&`bar me=baz", "not a very good name");
# Not even if there's a preexisting branch
test("atree.basic.3", $god, "&foo me=baz", "Set");
test("atree.basic.4", $god, "&foo` me=baz", "not a very good name");
# You may not have two ` in a row
test("atree.basic.5", $god, "&foo``bar me=baz", "not a very good name");
# Make a small tree
test("atree.basic.6", $god, "&foo me=baz", "Set");
test("atree.basic.7", $god, "&foo`bar me=baz", "Set");
test("atree.basic.8", $god, "&foo`bar`baz me=baz", "Set");
# Cannot clear branches with leaves until the leaves are cleared
test("atree.basic.9", $god, "&foo me", "!Cleared");
test("atree.basic.10", $god, "&foo`bar me", "!Cleared");
test("atree.basic.11", $god, "&foo`bar`baz me", "Cleared");
test("atree.basic.12", $god, "&foo`bar me", "Cleared");
test("atree.basic.13", $god, "&foo me", "Cleared");
# You can wipe, though.
test("atree.basic.14", $god, "&foo me=baz", "Set");
test("atree.basic.15", $god, "&foo`bar me=baz", "Set");
test("atree.basic.16", $god, "&foo`bar`baz me=baz", "Set");
test("atree.basic.17", $god, '@wipe me/foo', "wiped");

# Branch permissions
# May make a leaf without supporting branch
test("atree.branch.1", $god, "&foo`bar me=baz", "!You must set FOO first");
# And it must make the branch
test("atree.branch.2", $god, "think hasattr(me, foo)", "1");
# Another child should not wipe the previous values
test("atree.branch.3", $god, "&foo`bar`baz me=baz", "!You must set FOO first");
test("atree.branch.4", $god, "think get(me/foo`bar)", "baz");
# Clean up again
test("atree.branch.5", $god, '@wipe me/foo', "wiped");

# Wildcard attribute matching
# Rebuild a tree
test("atree.matching.1", $god, "&foo me=baz", "Set");
test("atree.matching.2", $god, "&foo`bar me=baz", "Set");
test("atree.matching.3", $god, "&foo`baz me=baz", "Set");
test("atree.matching.4", $god, "&foo`bar`baz me=baz", "Set");
# Examine should show a ` attribute flag for foo, foo`bar, but not foo`bar`baz
test("atree.matching.5", $god, "examine me/foo", 'FOO \[.*`\]');
test("atree.matching.6", $god, "examine me/foo`bar", 'FOO`BAR \[.*`\]');
test("atree.matching.7", $god, "examine me/foo`bar`baz", 'FOO`BAR`BAZ \[[^`]*\]');
# Examine doesn't show recursively, by default
test("atree.matching.8", $god, "examine me", ['FOO \[.*`\]', '!FOO`BAR']);
# But it will if you ask for it
test("atree.matching.9", $god, "examine me/**",
     ['FOO \[.*`\]', 'FOO`BAR \[', 'FOO`BAR`BAZ \[[^`]+\]']);
# If you ask for an attribute, you don't get its children
test('atree.matching.10', $god, 'examine me/FOO',
     ['FOO \[.*`\]', '!FOO`BAR \[', '!FOO`BAR`BAZ \[[^`]+\]']);
# You have to ask for the children
test('atree.matching.11', $god, 'examine me/FOO`',
     ['!FOO \[.*`\]', 'FOO`BAR \[', 'FOO`BAZ \[', '!FOO`BAR`BAZ \[[^`]+\]']);
test('atree.matching.12', $god, 'examine me/FOO`BAR`',
     ['!FOO \[.*`\]', '!FOO`BAR \[', '!FOO`BAZ \[', 'FOO`BAR`BAZ \[[^`]+\]']);
test('atree.matching.13', $god, 'examine me/FOO`*',
     ['!FOO \[.*`\]', 'FOO`BAR \[', 'FOO`BAZ \[', '!FOO`BAR`BAZ \[[^`]+\]']);
# A single * doesn't match `
test('atree.matching.14', $god, 'examine me/FOO*Z',
     ['!FOO \[.*`\]', '!FOO`BAR \[', '!FOO`BAZ \[', '!FOO`BAR`BAZ \[[^`]+\]']);
# A double * does match `
test('atree.matching.15', $god, 'examine me/FOO**Z',
     ['!FOO \[.*`\]', '!FOO`BAR \[', 'FOO`BAZ \[', 'FOO`BAR`BAZ \[[^`]+\]']);
# @decompile gets everything by default
test('atree.matching.16', $god, '@decompile me',
     ['&FOO ', '&FOO`BAR ', '&FOO`BAR`BAZ ']);
# But only the top layer if you say so
test('atree.matching.17', $god, '@decompile me/*',
     ['&FOO ', '!&FOO`BAR ', '!&FOO`BAR`BAZ ']);
# lattr() works like examine, only top by default
test('atree.matching.18', $god, 'think lattr(me)',
     ['\bFOO\b', '!\bFOO`BAR\b', '!\bFOO`BAR`BAZ\b']);
test('atree.matching.19', $god, 'think lattr(me/**)',
     ['\bFOO\b', '\bFOO`BAR\b', '\bFOO`BAR`BAZ\b']);
test("atree.matching.20", $god, 'think flags(me/foo)', '`');

# Permissions checks
# Need a mortal for this...
# Build a tree from different places...
test('atree.perms.2', $god, '&foo mortal=baz', 'Set');
test('atree.perms.3', $god, '&foo`bar mortal=baz', 'Set');
test('atree.perms.4', $mortal, '@decompile me', ['&FOO ', '&FOO`BAR ']);
test('atree.perms.5', $mortal, '&foo`bar me=baz', 'Set');
test('atree.perms.6', $mortal, '&foo`bar`baz me=baz', 'Set');
# Start flipping perms...
test('atree.perms.7', $god, '@set mortal/foo`bar=wiz', 'set');
test('atree.perms.8', $mortal, '@decompile me',
     ['&FOO ', '&FOO`BAR ', '&FOO`BAR`BAZ ']);
# Cannot overwrite wiz-only as mortal, or make stuff under it
test('atree.perms.9', $mortal, '&foo`bar me=baz', '!Set');
test('atree.perms.10', $mortal, '&foo`bar`baz me=baz', '!Set');
test('atree.perms.11', $mortal, '&foo`bar`qux me=baz', '!Set');
# Cannot see under mortal_dark as mortal
test('atree.perms.12', $god, '@set mortal/foo`bar=mortal_dark', 'set');
test('atree.perms.13', $mortal, '@decompile me',
     ['&FOO ', '!&FOO`BAR ', '!&FOO`BAR`BAZ ', '!&FOO`BAR`QUX ']);
# Still can't write there (still wiz-only)
test('atree.perms.14', $mortal, '&foo`bar me=baz', '!Set');
test('atree.perms.15', $mortal, '&foo`bar`baz me=baz', '!Set');
test('atree.perms.16', $mortal, '&foo`bar`qux me=baz', '!Set');
# Turn off wiz-only, but still can't see it...
test('atree.perms.17', $god, '@set mortal/foo`bar=!wiz', 'reset');
test('atree.perms.18', $mortal, '@decompile me',
     ['&FOO ', '!&FOO`BAR ', '!&FOO`BAR`BAZ ', '!&FOO`BAR`QUX ']);
# But you can write there again...
test('atree.perms.19', $mortal, '&foo`bar me=baz', 'Set');
test('atree.perms.20', $mortal, '&foo`bar`baz me=baz', 'Set');
test('atree.perms.21', $mortal, '&foo`bar`qux me=baz', 'Set');

# Parenting and ancestry
test('atree.parent.1', $mortal, '@create ancestor', 'Created');
test('atree.parent.2', $mortal, '@create parent', 'Created');
test('atree.parent.3', $mortal, '@create child', 'Created');
test('atree.parent.4', $mortal, 'drop child', '.');
test('atree.parent.5', $mortal, 'drop parent', '.');
test('atree.parent.6', $mortal, 'drop ancestor', '.');
test('atree.parent.7', $mortal, '@parent child=parent', 'Parent changed');
test('atree.parent.8', $god,
     '@config/set ancestor_thing=[after(num(ancestor),#)]', 'set');
# Can we see stuff from the ancestor?
test('atree.parent.9', $mortal, '&foo ancestor=urk', 'Set');
test('atree.parent.10', $mortal, '&foo`bar ancestor=urk', 'Set');
test('atree.parent.11', $mortal, '&foo`bar`baz ancestor=urk', 'Set');
test('atree.parent.12', $mortal, 'think get(child/foo)', 'urk');
test('atree.parent.13', $mortal, 'think get(child/foo`bar)', 'urk');
# Can we see stuff from the parent?
test('atree.parent.14', $mortal, '&foo parent=wibble', 'Set');
test('atree.parent.15', $mortal, '&foo`bar parent=gleep', 'Set');
test('atree.parent.16', $mortal, 'think get(child/foo)', 'wibble');
test('atree.parent.17', $mortal, 'think get(child/foo`bar)', 'gleep');
test('atree.parent.18', $mortal, '&foo`bar`baz child=boom', 'Set');
test('atree.parent.19', $mortal, 'think -[get(child/foo)]-', '--');
test('atree.parent.20', $mortal, 'think -[get(child/foo`bar)]-', '--');
test('atree.parent.21', $mortal, '@wipe child/foo', 'wiped');
# Setting no_inherit stops us inheriting at all
test('atree.parent.22', $mortal, '@set parent/foo`bar=no_inherit', 'set');
test('atree.parent.23', $mortal, 'think get(child/foo)', 'wibble');
test('atree.parent.24', $god, 'think get(child/foo`bar)', '^$');
test('atree.parent.25', $god, 'think get(child/foo)', 'wibble');
test('atree.parent.26', $god, 'think get(child/foo`bar)', '^$');

# Mix permissions and parents
# If parent is inheritable again, and mortal_dark,
# then we can't see the ancestor through it
test('atree.parentperms.1', $mortal, '@set parent/foo`bar=!no_inherit', 'set');
test('atree.parentperms.2', $god, '@set parent/foo`bar=mortal_dark', 'set');
test('atree.parentperms.3', $mortal, 'think get(child/foo`bar`baz)', '!urk');
# We can't see it, either
test('atree.parentperms.4', $mortal, 'think get(child/foo`bar)', '!gleep');
test('atree.parentperms.5', $mortal, '@set parent/foo=no_inherit', 'set');

# Command checks
# Need explicit grandparent, because ancestors aren't checked for commands
test('atree.command.1', $mortal, '@create grand', []);
test('atree.command.2', $mortal, 'drop grand', []);
test('atree.command.3', $mortal, '@parent parent=grand', []);
test('atree.command.4', $mortal, '&bar grand=$bar:say Grand Bar', 'Set');
test('atree.command.5', $mortal, '&bar`baz grand=$bar`baz:say Grand Baz', []);
test('atree.command.6', $mortal, '&bar parent=$bar:say Parent Bar', 'Set');
test('atree.command.7', $mortal, '&bar`baz parent=$bar`baz:say Parent Baz', []);
test('atree.command.8', $mortal, '@set child=!no_command', 'set');
# Do commands work from parent?
test('atree.command.9', $god, 'bar', '!Bar');
test('atree.command.10', $god, undef, 'Parent Bar');
test('atree.command.11', $god, 'bar`baz', []);
test('atree.command.12', $god, undef, 'Parent Baz');
# Child should block parent
test('atree.command.13', $mortal, '&bar child=$bar:say Child!', 'Set');
test('atree.command.14', $god, 'bar', '!Bar');
test('atree.command.15', $god, undef, ['!Bar', 'Child']);
# Child no_command blocks parent branch, too
test('atree.command.16', $mortal, '@set child/bar=no_command', 'set');
test('atree.command.17', $god, 'bar`baz', '!Baz');
test('atree.command.18', $god, undef, '!Baz');
# Parent no_command not masked by child not no_command...
test('atree.command.19', $mortal, '@set child/bar=!no_command', 'set');
test('atree.command.20', $mortal, '@set parent/bar=no_command', 'set');
test('atree.command.21', $god, 'bar`baz', '!Baz');
test('atree.command.22', $god, undef, '!Baz');
# no_command can be on the leaf, too
test('atree.command.23', $mortal, '@set parent/bar=!no_command', 'set');
test('atree.command.24', $mortal, '@set parent/bar`baz=no_command', 'set');
test('atree.command.25', $god, 'bar`baz', '!Baz');
test('atree.command.26', $god, undef, '!Baz');
# no_inherit trumps no_command
test('atree.command.27', $mortal, '@set parent/bar=no_inherit', 'set');
test('atree.command.28', $mortal, '@set parent/bar`baz=no_command', 'set');
test('atree.command.29', $god, 'bar`baz', '!Baz');
test('atree.command.30', $god, undef, 'Grand Baz');
test('atree.command.31', $mortal, '@set parent/bar=no_command', 'set');
test('atree.command.32', $mortal, '&bar child', []);
test('atree.command.33', $god, 'bar', '!Baz');
test('atree.command.34', $god, undef, 'Grand Bar');

# Test for the child recognition bugs:
test('atree.sortorder.1', $mortal, '&abc grand=$abc:say Grand ABC', 'Set');
test('atree.sortorder.2', $mortal, '&abcd grand=$abcd:say Grand D', 'Set');
test('atree.sortorder.3', $mortal, '&abc`xyz grand=$abc`xyz:say Grand XYZ', []);
test('atree.sortorder.4', $mortal, '&abc parent=$abc:say Parent ABC', 'Set');
test('atree.sortorder.5', $mortal, '&abcd parent=$abcd:say Parent D', 'Set');
test('atree.sortorder.6', $mortal, '&abc`xyz parent=$abc`xyz:say Parent XYZ', []);
test("atree.sortorder.7", $god, 'examine parent', 'ABC \[.*`\]');
test("atree.sortorder.8", $god, '&abc parent', '!Cleared');
test('atree.sortorder.9', $mortal, '@set child=!no_command', 'set');
# Do commands work from parent?
test('atree.sortorder.10', $god, 'abc', '!ABC');
test('atree.sortorder.11', $god, undef, 'Parent ABC');
test('atree.sortorder.12', $god, 'abc`xyz', []);
test('atree.sortorder.13', $god, undef, 'Parent XYZ');
# Child should block parent
test('atree.sortorder.14', $mortal, '&abc child=$abc:say Child!', 'Set');
test('atree.sortorder.15', $god, 'abc', '!ABC');
test('atree.sortorder.16', $god, undef, ['!ABC', 'Child']);
# Child no_command blocks parent branch, too
test('atree.sortorder.17', $mortal, '@set child/abc=no_command', 'set');
test('atree.sortorder.18', $god, 'abc`xyz', '!XYZ');
test('atree.sortorder.19', $god, undef, '!XYZ');
# Parent no_command not masked by child not no_command...
test('atree.sortorder.20', $mortal, '@set child/abc=!no_command', 'set');
test('atree.sortorder.21', $mortal, '@set parent/abc=no_command', 'set');
test('atree.sortorder.22', $god, 'abc`xyz', '!XYZ');
test('atree.sortorder.23', $god, undef, '!XYZ');
# no_command can be on the leaf, too
test('atree.sortorder.24', $mortal, '@set parent/abc=!no_command', 'set');
test('atree.sortorder.25', $mortal, '@set parent/abc`xyz=no_command', 'set');
test('atree.sortorder.26', $god, 'abc`xyz', '!XYZ');
test('atree.sortorder.27', $god, undef, '!XYZ');
# no_inherit trumps no_command
test('atree.sortorder.28', $mortal, '@set parent/abc=no_inherit', 'set');
test('atree.sortorder.29', $mortal, '@set parent/abc`xyz=no_command', 'set');
test('atree.sortorder.30', $god, 'abc`xyz', '!XYZ');
test('atree.sortorder.31', $god, undef, 'Grand XYZ');
test('atree.sortorder.32', $mortal, '@set parent/abc=no_command', 'set');
test('atree.sortorder.33', $mortal, '&abc child', []);
test('atree.sortorder.34', $god, 'abc', '!XYZ');
test('atree.sortorder.35', $god, undef, 'Grand ABC');
# wipe check
test("atree.sortorder.36", $god, '@wipe parent', 'wiped');
test("atree.sortorder.37", $god, '@wipe grand/abc', []);
test("atree.sortorder.38", $god, 'examine grand/**', '!ABC\b');
