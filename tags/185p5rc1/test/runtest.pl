#!/usr/bin/perl -w
use strict;
use Getopt::Long;
use PennMUSH;
use TestHarness;

my ($valgrind, $host, $port) = (0,"localhost",0);
GetOptions "valgrind" => \$valgrind,
    "host" => \$host,
    "port" => \$port;

my $mush = PennMUSH->new($host, $port, $valgrind);

my @tests = map { TestHarness->new($_); } @ARGV;

my $god = $mush->loginGod;

my $mortal = undef;
if ($TestHarness::use_mortal) {
    $god->command('@pcreate Mortal=mortal');
    $mortal = $mush->login("Mortal", "mortal");
}

foreach my $test (@tests) {
    $test->run($god, $mortal);
}
