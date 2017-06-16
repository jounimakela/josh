josh: main.c
	$(CC) main.c -o josh -Wall -Wextra -Wshadow -pedantic -std=c11 -ggdb -fno-strict-aliasing -O0

clean:
	rm josh
