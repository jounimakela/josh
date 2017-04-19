josh: main.c
	$(CC) main.c -o josh -Wall -Wextra -pedantic -std=c11 -ggdb

clean:
	rm josh
