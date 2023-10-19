clang -c -Wall -Wshadow -O2 -Wl,-z,notext tinyexpr.c -o tinyexpr.o
clang -c -Wall -Wshadow -O2 -Wl,-z,notext example.c -o example.o
clang -Wall -Wshadow -O2 -Wl,-z,notext -o example example.o tinyexpr.o -lm
clang -shared -fPIC -Wl,-z,notext -O2 -I../../ -I../../hdrs -I../../pcre2/include -o ../math.so math.c tinyexpr.o -lm
