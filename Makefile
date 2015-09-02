CC=gcc

all:
	$(CC) -o deem deem.c

test:
	echo a 1 b 3 d 4 | ./deem test.a
	echo a 1 b 2 c 3 | ./deem test.a
	echo Expecting: a 1 b 2 c 3
	ar pv test.a | sed "s/[<\>]//g" | xargs
	rm test.a