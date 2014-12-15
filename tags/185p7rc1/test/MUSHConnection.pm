package MUSHConnection;

# use strict;
use IO::Poll;
use IO::Socket::INET;

my $nextpat = "PATTERN000000001";

sub new {
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my $self = [];
  $self->[0] = IO::Socket::INET->new();
  $self->[1] = {};
  $self->[1]->{PREFIX} = '=-=-= OUTPUTPREFIX =-=-=';
  $self->[1]->{SUFFIX} = '=-=-= OUTPUTSUFFIX =-=-=';
  $self->[1]->{MATCHER} = {};
  bless($self, $class);
  $self->connect(@_) if @_;
  return $self;
}

sub connected {
  my $self = shift;

  my $socket = $self->[0];
  return $socket->connected();
}

sub connect {
  my $self = shift;
  my $addr = shift;
  my $port = shift;
  my $name = shift;
  my $passwd = shift;


  my $socket = $self->[0];
#  $socket->close if $socket->connected();
  $self->[0] = IO::Socket::INET->new(PeerAddr => $addr, PeerPort => $port,
                                     Proto => "tcp");
  $socket = $self->[0];
#  $socket->connect(PeerAddr => $addr, PeerPort => $port, Proto => "tcp");
  $socket->autoflush(1);
  $socket->timeout(30);

  $self->read_to_pattern('.') || return;
  $self->read_to_empty();
  $socket->print("connect $name $passwd\r\n");
  $socket->flush();
  $self->read_to_pattern('.') || return;
  $self->read_to_empty();
  sleep(1);
  $socket->print("OUTPUTPREFIX " . $self->[1]->{PREFIX} . "\r\n");
  $socket->print("OUTPUTSUFFIX " . $self->[1]->{SUFFIX} . "\r\n");
  $socket->print("say CodeMUSH $$\r\n");
  $self->read_to_pattern("CodeMUSH $$") || return;
}

sub disconnect {
  my $self = shift;

  my $socket = $self->[0];
  $socket->close if $socket->connected();
}

sub read_to_pattern {
  my $self = shift;
  my $pattern = shift;

# warn "Looking for pattern $pattern\n";
  my $matcher = $self->[1]->{MATCHER}->{$pattern};
  unless ($matcher) {
    my $patsub = $pattern;
#    $patsub =~ s/(\W)/\\$1/go;
    my $sub = <<EOT;
sub $nextpat {
  return (\$`, \$&, \$') if \$_[0] =~ /$patsub/o;
  return undef;
}
1;
EOT
# warn "Building matcher $nextpat:\n$sub";
    eval($sub);
    $matcher = $nextpat++;
    $self->[1]->{MATCHER}->{$pattern} = $matcher;
  }
# warn "Using matcher $matcher\n";

  my $socket = $self->[0];
  my $buffer = $self->[1]->{BUFFER};
  my @match = $buffer ? &$matcher($buffer) : undef;
  my $poll = new IO::Poll;
  $poll->mask($socket => POLLIN | POLLERR | POLLHUP);
  until (@match > 1) {
# warn "Looping...\n";
    my $buf;
    my $amount = $socket->sysread($buf, 1024);
# warn "Read $amount: $buf...\n";
    $amount || ($self->disconnect(), return);
    $buffer .= $buf;
  } continue {
    @match = &$matcher($buffer);
  }
  $self->[1]->{BUFFER} = $match[2];

# warn "Found match: ".join(",", @match)."\n";
# warn "Returning: ".join(",",@match[0,1])."\n";
  return (@match[0,1]);
}

sub read_to_empty {
  my $self = shift;

# warn "Emptying input...\n";
  my $socket = $self->[0];
  my $poll = new IO::Poll;
  $poll->mask($socket => POLLIN | POLLERR | POLLHUP);
  my $result = $self->[1]->{BUFFER};
  my $buf;
  while ($poll->poll(0) && !($poll->events($socket) & POLLERR | POLLHUP)) {
    $socket->read($buf, 1024, 0);
    $result .= $buf;
  }
  $self->[1]->{BUFFER} = "";
# warn "Have result: $result\n";
  return $result;
}

sub command {
  my $self = shift;
  my $command = shift;
  my $socket = $self->[0];
  my $noise = $self->read_to_empty();
  $socket->print($command."\r\n");
  my @result = $self->read_to_pattern($self->[1]->{PREFIX});
  $noise .= $result[0];
  $self->[1]->{NOISE} = $noise;
  @result = $self->read_to_pattern($self->[1]->{SUFFIX});
  $result[0] =~ s/^[\r\n]+//o;
# warn "Noise: $noise\n";
  return $result[0];
}

sub noise {
  my $self = shift;
  return $self->[1]->{NOISE};
}

sub listen {
  my $self = shift;
  $self->command("think Listening!");
  $self->[1]->{NOISE} =~ s/^\r?\n//o;
# warn "LISTENING!: ".$self->[1]->{NOISE}."\n";
  return $self->[1]->{NOISE};
}

1;
