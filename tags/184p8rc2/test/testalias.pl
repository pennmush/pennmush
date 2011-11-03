run tests:
test('alias.1', $god, '@name me=God', ['Name set.']);
test('alias.2', $god, '@name me=One', ['Name set.']);
test('alias.3', $god, '@alias me=God', ['Alias set.']);
test('alias.4', $god, '@name me=God', ['Name set.']);
test('alias.5', $god, '@alias me=God', ['Alias set.']);
test('alias.6', $god, '@name me=One', ['Name set.']);
test('alias.7', $god, '@name me=God', ['Name set.']);
test('alias.8', $god, '@alias me=God;One', ['Alias set.']);
test('alias.9', $god, '@name me=Love', ['Name set.']);
test('alias.10', $god, '@alias me', ['Alias removed.']);
test('alias.11', $god, '@name me=God', ['Name set.']);
