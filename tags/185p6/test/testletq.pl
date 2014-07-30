run tests:
test("letq.1", $god, "think setr(A, 1):[letq(A, 2, \%qA)]:\%qA", '^1:2:1');
test("letq.2", $god, "think setr(A, 1):[letq(setr(A, 2))]:\%qA", '^1:2:2');
