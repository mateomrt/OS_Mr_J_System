all: Fleck Gotham Harley Enigma

Fleck: Fleck.c
	gcc -Wall -g -o Fleck Fleck.c Strings.c

Gotham: Gotham.c
	gcc -Wall -g -o Gotham Gotham.c Strings.c

Harley: Harley.c
	gcc -Wall -g -o Harley Harley.c Strings.c

Enigma: Enigma.c
	gcc -Wall -g -o Enigma Enigma.c Strings.c

clean:
	rm -f Fleck Gotham Harley Enigma