package PennMUSH;

use File::Copy;
use File::Path;
use MUSHConnection;

my @pids = ();

sub new {
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my $self = {};
  bless($self, $class);
  if (@_) {
      $self->{HOST} = shift;
      $self->{PORT} = shift;
      $self->{VALGRIND} = shift;
      $self->start(@_);
  } else {
    $self->start();
  }

  return $self;
}

sub start {
  my $self = shift;
  srand();
  $self->{HOST} = "localhost" unless defined $self->{HOST};
  if (!exists $self->{PORT} || $self->{PORT} <= 0) {      
      $self->{PORT} = int(rand(2000)) + 12000;
  }
  my $port = $self->{PORT};
  rmtree("testgame");
  mkpath(["testgame/data", "testgame/log", "testgame/txt"]);
  copyConfig("../game/mushcnf.dst", "testgame/test.cnf",
             "port" => $port,
             "compress_program" => "",
             "uncompress_program" => "",
             "compress_suffix" => "",
             @_);
  copy("../game/alias.cnf", "testgame/alias.cnf");
  copy("../game/names.cnf", "testgame/names.cnf");
  copy("../game/restrict.cnf", "testgame/restrict.cnf");
  my $file;
  foreach $file (glob("../game/txt/*.txt")) {
    my $target = $file;
    $target =~ s-../game-testgame-o;
    copy($file, $target);
  }
  symlink("../../src/netmud", "testgame/netmush");
  symlink("../../src/info_slave", "testgame/info_slave");
  my $child = fork();
  if ($child > 0) {
    my $j;
    my $line;
    push(@pids, $child);
    $self->{PID} = $child;
    foreach $j (1..20) {
      next unless open(LOG, "testgame/log/netmush.log");
      while ($line = <LOG>) {
        close(LOG), return $port if $line =~ /^Listening on port $port /;
      }
    } continue {
      sleep(1);
    }
    die "Could not start game process properly; pid $child!\n";
  } elsif (defined($child)) {
    chdir("testgame");
    my @execargs = ("./netmush", "--no-session", "test.cnf");
    unshift @execargs, "valgrind", "--tool=memcheck", '--log-file=../valgrind-%p.log',
                       "--leak-check=full", "--track-origins=yes"
	if $self->{VALGRIND};
    exec @execargs;
  } else {
    die "Could not spawn game process!\n";
  }
}

sub copyConfig {
  my $from = shift;
  my $to = shift;
  my %subs = @_;

  open(FROM, "<$from") || die "Could not open template configuration.\n";
  open(TO, ">$to") || die "Could not write test configuration.\n";
  my $line;
  while ($line = <FROM>) {
    next if $line =~ /^\s*#/o;
    next unless $line =~ /^\s*(\w+)\s/o;
    my $key = $1;
    $line = $key . " " . $subs{$key} . "\n" if defined($subs{$key});
  } continue {
    print TO $line;
  }
  close(TO);
  close(FROM);
}

sub login {
  my $self = shift;
  return MUSHConnection->new($self->{HOST}, $self->{PORT}, @_);
}

sub loginGod {
  my $self = shift;
  return MUSHConnection->new($self->{HOST}, $self->{PORT}, "One", "one");
}

END {
  kill("INT", @pids);
}

1;
