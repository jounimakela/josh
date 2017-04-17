#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

#define CTRL(k) ((k) & 0x1f)

struct termios orig_termios;

void die(const char *s)
{
	perror(s);
	exit(1);
}

void tty_restore()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}

void tty_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		die("tcgetattr");

	atexit(tty_restore);

	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

char read_key()
{
	char c;
	while (read(STDIN_FILENO, &c, 1) != 1);
	return c;
}

void process_key()
{
	char c = read_key();

	switch(c) {
		case CTRL('q'):
			exit(0);
			break;

		case CTRL('h'):
			write(STDOUT_FILENO, "\x1b[1D", 4);
			break;

		case CTRL('l'):
			write(STDOUT_FILENO, "\x1b[1C", 4);
			break;

		default:
			if (isprint(c)) {
				printf("%c", c);
				fflush(stdout);
			}
			break;
	}
}

int main()
{
	tty_raw_mode();

	while (1) {
		process_key();
	}

	return 0;
}
