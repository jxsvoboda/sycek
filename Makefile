CC     = gcc
CFLAGS = -std=c99 -D_GNU_SOURCE -O0 -ggdb -Wall -Wextra -Werror -I.
LIBS   =

bkqual = $$(date '+%Y-%m-%d')

sources = \
    lexer.c \
    main.c \
    test/lexer.c

binary = cstyle

objects = $(sources:.c=.o)
headers = $(wildcard *.h */*.h */*/*.h)

all: $(binary)

$(binary): $(objects)
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

$(objects): $(headers)

clean:
	rm -f $(objects) $(binary)

backup: clean
	cd .. && tar czf cstyle-$(bkqual).tar.gz trunk
	cd .. && rm -f cstyle-latest.tar.gz && ln -s cstyle-$(bkqual).tar.gz cstyle-latest.tar.gz
