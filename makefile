all: Fleck Gotham Harley Enigma

Fleck: Fleck.c
	gcc -Wall -g -o Fleck Fleck.c Common.c

Gotham: Gotham.c
	gcc -Wall -g -o Gotham Gotham.c Common.c

Harley: Harley.c
	gcc -Wall -g -o Harley Harley.c Common.c

Enigma: Enigma.c
	gcc -Wall -g -o Enigma Enigma.c Common.c

clean:
	rm -f Fleck Gotham Harley Enigma