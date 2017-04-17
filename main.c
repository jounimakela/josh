#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

struct termios orig_termios;

void tty_restore()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void tty_raw_mode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(tty_restore);

	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* raw.c_oflag &= ~(OPOST); */
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
	tty_raw_mode();

	char c;
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if (iscntrl(c)) {
			printf("%d\n", c);
		} else {
			printf("%d ('%c')\n", c, c);
		}
	}
	return 0;
}
