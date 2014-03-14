#!/usr/bin/env python2.7
"""Simple fuzzer for the flag code in PennMUSH.

Potential improvements in the future include:
- Testing invalid flag names
- Testing setting/unsetting flags with insufficient permissions
"""

import random
import socket
import sys


# This is actually a subset of the namespace for valid flag names. ` and = are
# not considered since they have special error cases and parsing rules,
# respectively.
_VALID_FLAG_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_#@$!~|;\'"&*-+?/.><,'
_MAX_FLAG_LENGTH = 32  # So the log lines aren't too ridiculously long.
# To fuzz the invalid permission handling, uncomment 'internal' and 'dark'.
_VALID_PERMISSIONS = [
  'trusted',
  'owned',
  'royalty',
  # 'wizard',  # Currently always included so unsettable flags aren't created.
  'god',
  # 'internal',
  'dark',
  'mdark',
  'odark',
  # 'disabled',
  'log',
  'event',
]
_VALID_TYPES = [
  'player',
  'room',
  'thing',
  'exit',
]

_HOST = '127.0.0.1'
_PORT = 4201
_ARBITRARY_TIMEOUT = 0.5

_LOGIN_STRING = 'connect one\n'
_DBREFS = {
  'player': '#1',
  'room': '#0',
  'thing': '#3',
  'exit': '#7',
}


def _connect_to_game():
    """Connects to the game... hopefully.

    Returns:
      A socket that's hopefully connected to aforementioned game.
    """
    print '=== Attempting to connect to game...'
    game_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    game_socket.connect((_HOST, _PORT))
    game_socket.settimeout(_ARBITRARY_TIMEOUT)
    game_socket.sendall(_LOGIN_STRING)
    _read_lines_until_idle(game_socket)
    print '=== Probably connected.'
    return game_socket


def _read_lines_until_idle(game_socket):
    """Attempts to read lines from the socket.

    It returns when reads start hitting the timeout. Hopefully this means the
    socket is idle and ready for more fuzzing!

    Args:
        game_socket: A socket that's connected to the game.
    """
    try:
        socket_file = game_socket.makefile()
        while True:
            sys.stdout.write(socket_file.readline())
    except socket.timeout:
        return


def _generate_random_flag_name():
    """Generates a random valid flag name."""
    length = random.randrange(2, _MAX_FLAG_LENGTH)
    return ''.join([random.choice(_VALID_FLAG_CHARS) for _ in xrange(length)])


def _generate_random_types():
    """Generates a random set of types that the flag can be set on."""
    length = random.randrange(len(_VALID_TYPES))
    shuffled = _VALID_TYPES
    random.shuffle(shuffled)
    return ' '.join(shuffled[:length])


def _generate_random_permissions():
    """Generates random permissions for a flag.

    Note that the set of random permissions currently always includes the wizard
    permission so that an unsettable flag isn't created.
    """
    length = random.randrange(len(_VALID_PERMISSIONS))
    shuffled = _VALID_PERMISSIONS
    random.shuffle(shuffled)
    shuffled = shuffled[:length]
    # TODO: Rather than doing this, make sure one of royalty/wizard/god is
    # always included in the generated permissions.
    shuffled.append('wizard')
    return ' '.join(shuffled)


def _fuzz_flag_add(game_socket, flags):
    """Fuzzing helper to add a flag to the game.
    
    Args:
        game_socket: A socket that's connected to the game.
        flags: A dictionary mapping flags to their valid types.
    """
    flag = ''
    while True:
        flag = _generate_random_flag_name()
        if flag not in flags:
            break
    types = _generate_random_types()
    setters = _generate_random_permissions()
    unsetters = _generate_random_permissions()
    print '=== Attempting to add flag %s' % flag
    game_socket.sendall(
        '@flag/add %s=,%s,%s,%s\n' % (flag, types, setters, unsetters))
    _read_lines_until_idle(game_socket)
    flags[flag] = types.split()


def _fuzz_flag_remove(game_socket, flags):
    """Fuzzing helper to remove a flag from the game.
    
    Args:
        game_socket: A socket that's connected to the game.
        flags: A dictionary mapping flags to their valid types.
    """
    if not flags:
        return
    flag = random.choice(flags.keys())
    print '=== Attempting to remove flag %s' % flag
    game_socket.sendall('@flag/delete %s\n' % flag)
    _read_lines_until_idle(game_socket)
    del flags[flag]


def _fuzz_flag_set(game_socket, flags):
    """Fuzzing helper to set/unset a flag in the game.
    
    Args:
        game_socket: A socket that's connected to the game.
        flags: A dictionary mapping flags to their valid types.
    """
    if not flags:
        return
    flag = random.choice(flags.keys())
    dbref = ''
    if flags[flag]:
        dbref = _DBREFS[random.choice(flags[flag])]
    else:
        dbref = _DBREFS[random.choice(_DBREFS.keys())]
    enabling = random.randrange(2)
    if enabling:
        print '=== Attempting to set flag %s' % flag
        game_socket.sendall('@set %s=%s\n' % (dbref, flag))
    else:
        print '=== Attempting to unset flag %s' % flag
        game_socket.sendall('@set %s=!%s\n' % (dbref, flag))
    _read_lines_until_idle(game_socket)


def main():
    """The main function!"""
    random.seed(0)  # Deterministic seed for now.
    game_socket = _connect_to_game()
    flags = {}
    while True:
        random_choice = random.randrange(3)
        if random_choice == 0:
            _fuzz_flag_add(game_socket, flags)
        elif random_choice == 1:
            _fuzz_flag_remove(game_socket, flags)
        elif random_choice == 2:
            _fuzz_flag_set(game_socket, flags)


if __name__ == '__main__':
    sys.exit(main())
