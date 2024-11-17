all: Fleck Gotham Harley Enigma

Fleck: Fleck.c
	gcc -Wall -g -o Fleck Fleck.c Common.c Protocol.c -lpthread

Gotham: Gotham.c
	gcc -Wall -g -o Gotham Gotham.c Common.c Protocol.c -lpthread

Harley: Harley.c
	gcc -Wall -g -o Harley Harley.c Common.c Protocol.c -lpthread

Enigma: Enigma.c
	gcc -Wall -g -o Enigma Enigma.c Common.c Protocol.c -lpthread

clean:
	rm -f Fleck Gotham Harley Enigma