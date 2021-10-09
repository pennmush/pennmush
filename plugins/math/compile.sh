gcc -c -Wall -Wshadow -O2 tinyexpr.c -o tinyexpr.o
gcc -c -Wall -Wshadow -O2 example.c -o example.o
gcc -Wall -Wshadow -O2 -o example example.o tinyexpr.o -lm
gcc -O2 -shared -fPIC -I../../ -I../../hdrs -I../../pcre2/include -o ../math.so math.c tinyexpr.o -lm
