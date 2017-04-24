josh: main.c
	$(CC) main.c -o josh -Wall -Wextra -Wshadow -pedantic -std=c11 -ggdb -fno-strict-aliasing

clean:
	rm josh
