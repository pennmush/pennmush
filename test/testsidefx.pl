# Test side effect functions and commands.

run tests:

# @clone and clone().
# TO-DO: Test the dbref argument.
test('clone.1', $god, '@create Original', "Created");
test('clone.2', $god, '@set Original=Wizard', "WIZARD set");
test('clone.3', $god, '&FOO Original=blah', "Original/FOO - Set.");
test('clone.4', $god, '@clone Original=Copy1', "Cloned");
test('clone.5', $god, 'think hasflag(Copy1, WIZARD)', '^0');
test('clone.6', $god, 'think hasattr(Copy1, FOO)', '^1');
test('clone.7', $god, '@clone/preserve Original=Copy2', "Cloned");
test('clone.8', $god, 'think hasflag(Copy2, WIZARD)', '^1');
test('clone.9', $god, 'think clone(Original, Copy3)', "Cloned");
test('clone.10', $god, 'think hasflag(Copy3, WIZARD)', '^0');
test('clone.11', $god, 'think clone(Original, Copy4, , preserve)', "Cloned");
test('clone.12', $god, 'think hasflag(Copy4, WIZARD)', '^1');
test('clone.13', $god, '@clone NoSuchObject', "I can't see that here.");
test('clone.14', $god, 'think clone(NoSuchObject)', "I can't see that here.");
