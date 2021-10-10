gcc -g -c -Wall -Wshadow tinyexpr.c -o tinyexpr.o
gcc -c -Wall -Wshadow example.c -o example.o
gcc -g -Wall -Wshadow -o example example.o tinyexpr.o -lm
gcc -g -shared -fPIC -I../../ -I../../hdrs -I../../pcre2/include -o ../math.so math.c tinyexpr.o -lm
