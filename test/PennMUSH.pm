package PennMUSH;
use strict;
use warnings;
use File::Copy;
use File::Path;
use List::Util qw/any/;
use MUSHConnection;
use POSIX qw/:sys_wait_h/;
use feature qw/say/;

my @pids = ();

$SIG{"CHLD"} = sub  {
  while ((my $child = waitpid(-1, WNOHANG))  > 0) {
    if (any { $_ == $child } @pids) {
      my $status = ${^CHILD_ERROR_NATIVE};
      my $errmsg = "Child PennMUSH process $child exited: ";
      if (WIFEXITED($status)) {
        $errmsg .= "Exit code " . WEXITSTATUS($status);
      } elsif (WIFSIGNALED($status)) {
        $errmsg .= "Killed with signal " . WTERMSIG($status);
      }
      say $errmsg;
      say "Last lines of log:";
      say `tail log/netmush.log`;
      say "Valgrind:";
      say `cat valgrind-*`;
    }
  }
};

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
	     "mem_check" => "yes",
	     "dict_file" => "",
             @_);
  copy("../game/alias.cnf", "testgame/alias.cnf");
  copy("../game/names.cnf", "testgame/names.cnf");
  copy("../game/restrict.cnf", "testgame/restrict.cnf");
  copy("../game/txt/colors.json", "testgame/txt/colors.json");
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
    sleep 5;
    foreach $j (1..10) {
      next unless open my $LOG, "<", "testgame/log/netmush.log";
      while ($line = <$LOG>) {
        return $port if $line =~ /Listening on port $port /;
      }
      close $LOG;
    } continue {
      sleep ($self->{VALGRIND} ? 10 : 5);
    }
    die "Could not start game process properly; pid $child!\n";
  } elsif (defined($child)) {
    chdir("testgame");
    my @execargs = ("./netmush", "--no-session", "--disable-socket-quota", "--tests");
    if ($self->{VALGRIND}) {
      unshift @execargs, "valgrind", "--tool=memcheck", '--log-file=../valgrind-%p.log',
	"--leak-check=full", "--track-origins=yes";
      push @execargs, "--no-pcre-jit";
    }
    push @execargs, "test.cnf";
    exec @execargs;
    die "Unable to run '@execargs': $!\n";
  } else {
    die "Could not spawn game process: $!\n";
  }
}

sub copyConfig {
  my $from = shift;
  my $to = shift;
  my %subs = @_;

  open my $FROM, "<", $from or die "Could not open template configuration: $!\n";
  open my $TO, ">", $to or die "Could not write test configuration: $!\n";
  my $line;
  while ($line = <$FROM>) {
    next if $line =~ /^\s*#/o;
    next unless $line =~ /^\s*(\w+)\s/o;
    my $key = $1;
    $line = $key . " " . $subs{$key} . "\n" if defined($subs{$key});
  } continue {
    print $TO $line;
  }
  close $TO;
  close $FROM;
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
