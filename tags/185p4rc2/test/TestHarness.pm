package TestHarness;
use strict;
use vars qw/%tests $testcount @failures $alltests @allfailures $allexpected/;
use vars qw/$testfiles $use_mortal/;
use subs qw/test summary/;

$alltests = 0;
@allfailures = ();
$allexpected = 0;
$testfiles = 0;
$use_mortal = 0;

sub new {
    my $class = shift;
    my $script = shift;
    my %self = ( 
        -expected => 0,
        -depends => [],
        -test => undef,
    );
#    print "Looking at $script\n";
    $script =~ /^test(.*)\.pl$/o;
    my $name = $1;
    $self{-name} = $name;
    warn "Duplicate test $name\n" if exists $tests{$name};
    my $code = 'sub { my $god = shift; ' . "\n";
    open IN, "<", $script or die "Couldn't open ${script}: $!\n";
    while (<IN>) {
        chomp;
        next if /^\s*(?:#|$)/o;
        last if /^run tests:$/o;
        if (/^depends on (.*)$/o) {
            push @{$self{-depends}}, $1;
        }
        if (/^expect (\d+) failures!$/) {
            $self{-expected} = $1;
            print "Expecting $1 failures in $name\n";
        }
        if (/^\s*login mortal$/) {
            $code .= 'my $mortal = shift;' . "\n";
            $use_mortal = 1;
        }
    }
    while (<IN>) {
        $code .= $_;
    }
    close IN;
    $code .= "}\n";
#    print "Test function for $name:\n$code\n";
#    flush STDOUT;
    $self{-test} = eval $code;
    my $obj = bless \%self;
    $tests{$name} = $obj;
    return $obj;
}

sub run {
    my $self = shift;
    my $god = shift;
    my $mortal = shift;
    my ($failures, $test) = (0,0);

    foreach my $dep (@{$self->{-depends}}) {
        my $test = $tests{$dep};
        if (defined $test) {
            $test->run($god, $mortal);
        } else {
            warn "Unresolved dependency $dep\n";
        }
    }
    
    local ($testcount, @failures) = (0, ());

    my $name = $self->{-name};

    print "Running tests for ${name}:\n";

    &{$self->{-test}}($god, $mortal);

    $testfiles++;
    $alltests += $testcount;
    push @allfailures, @failures;
    $allexpected += $self->{-expected};
    
    summary $self->{-name}, $testcount, \@failures, $self->{-expected};
}

END {
    print "Totals:\n";
    summary("all tests run", $alltests, \@allfailures, $allexpected)
        if $testfiles > 1;
}

$| = 1;

sub test {
  my $name = shift;
  my $conn = shift;
  my $command = shift;
  my $patterns = shift;

  $patterns = [$patterns] if ref($patterns) ne "ARRAY";

  print substr("Running $name".(" "x80), 0, 78)."\r";

  my $result = defined($command) ? $conn->command($command) : $conn->listen();
  my $verdict = 1;

  foreach my $pattern (@$patterns) {
    my $matchpattern = $pattern;
    my $negate = 0;
    if ($matchpattern =~ s/^!//o) {
      $negate = 1;
    } else {
      $matchpattern =~ s/^=//o;
    }

    $result =~ s/\s+$//o;

    if ($negate) {
      $verdict = 0 if $result =~ /$matchpattern/;
    } else {
      $verdict = 0 unless $result =~ /$matchpattern/;
    }
  }

  $testcount++;
  unless ($verdict) {
    push(@failures, $name);
    print "TEST FAILURE: $name\n";
    if (defined($command)) {
      print "  command: $command\n";
    } else {
      print "  listening\n";
    }
    chomp $result;
    if ($result =~ /\n/o) {
      print "  result:\n$result\n";
    } else {
      print "  result:  $result\n";
    }
    foreach my $pattern (@$patterns) {
      print "  pattern: $pattern\n";
    }
    print "\n";
  }
}

sub summary {
    my ($name, $testcount, $failures, $expected) = @_;
    print ":"x70, "\n";
    print "\n";
    my $fcount = 0;
    if (ref $failures) {
        $fcount = scalar @$failures;
    } else {
        $fcount = $failures;
    }
    my $scount = $testcount - $fcount;
    print "$testcount tests, $scount succeeded, $fcount failed ($expected expected failures)\n";
    if ($fcount != $expected) {
        print "failed tests:\n";
        my $str = join(", ", @$failures);
        while (length($str) > 67) {
            $str =~ s/^(.{1,67}), //o;
            print "  $1,\n";
        }
        print "  $str\n";
    }
    print "\n";
}

1;
