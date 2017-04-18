#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

#define CTRL(k) ((k) & 0x1f)

#define ABUF_INIT {NULL, 0}

struct termios orig_termios;

struct line {
	char *buf;		/* Edited line buffer		*/
	size_t buflen;		/* Edited line buffer size	*/
	const char *prompt;	/* Prompt to display		*/
	size_t prompt_len;	/* Prompt length		*/
	size_t pos;		/* Current cursor position	*/
	size_t len;		/* Current edited line length	*/
};

struct abuf {
	char *buf;
	int len;
};

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

void buf_append(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->buf, ab->len + len);

	if (new == NULL)
		return;

	memcpy(&new[ab->len], s, len);
	ab->buf = new;
	ab->len = len;
}

void buf_free(struct abuf *ab)
{
	free(ab->buf);
}

void refresh_line(struct line *l)
{
	char content[64];
	struct abuf ab = ABUF_INIT;

	/* Move cursor to left */
	snprintf(content, 64, "\r");
	buf_append(&ab, content, strlen(content));

	/* Write prompt and buffer content */
	buf_append(&ab, l->prompt, l->prompt_len);
	buf_append(&ab, l->buf, l->len);

	/* Move cursor to original position */
	snprintf(content, 64, "\r\x1b[%dC", (int)(l->pos + l->prompt_len));
	buf_append(&ab, content, strlen(content));

	write(STDOUT_FILENO, ab.buf, ab.len);

	buf_free(&ab);
}

void cur_move_right(struct line *l)
{
	if (l->pos != l->len)
	{
		l->pos++;
		refresh_line(l);
	}
}

void cur_move_left(struct line *l)
{
	if (l->pos > 0)
	{
		l->pos--;
		refresh_line(l);
	}
}

char read_key()
{
	char c;
	while (read(STDIN_FILENO, &c, 1) != 1);
	return c;
}

void process_key(struct line *l)
{
	char c = read_key();

	switch(c) {
		case CTRL('d'):
			exit(0);
			break;

		case CTRL('h'):
			cur_move_left(l);
			/* write(STDOUT_FILENO, "\x1b[1D", 4); */
			break;

		case CTRL('l'):
			cur_move_right(l);
			/* write(STDOUT_FILENO, "\x1b[1C", 4); */
			break;

		default:
			if (isprint(c)) {
				break;
			}
			break;
	}
}

int main()
{
	tty_raw_mode();

	struct line l;

	char buf[64];
	l.buf = buf;
	l.buflen = 64;
	l.prompt = "test";
	l.prompt_len = strlen("test");

	refresh_line(&l);

	/*
	while (1) {
		process_key(&l);
	}
	*/

	return 0;
}
