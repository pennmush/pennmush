Raevnos is currently in the middle of converting the Penn build system
from autoconf and hand written makefiles to using cmake to generate
project files for arbitrary environments.

Currently, only the "Unix Makefiles" generator is being tested.

Instructions for use (Subject to change):

1. Configure the system and create build files:
   % cmake -DCMAKE_BUILD_TYPE=Debug src

  Or use Release, RelWithDebInfo or MinSizeRel.

  Additional options that can be passed to cmake via -D NAME=1 include:

  NO_INFO_SLAVE  -- disable info_slave
  NO_SSL_SLAVE   -- disable ssl_slave
  USE_CLANG_TIDY -- Lint using clang-tidy when compiling.

  You can undo the effects of these options by running
  % cmake -U NAME src

2. Build:
  % cmake --build . -- [make options like -j8]
  % cmake --build . -- install update

Other targets:

clean -- remove compiled files etc.
versions -- rebuild help versions of changelogs.

Extra optional targets created depending on what's found by cmake
during the configuration step:

etags -- build an emacs TAGS file.
ctags -- build a vi tags file.
indent -- run clang-format over the source.

