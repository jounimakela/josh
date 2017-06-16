#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

#define ABUF_INIT {NULL, 0}

#define HISTORY_MAX_ITEMS       1024
#define LINE_MAX_LENGTH         256

struct termios orig_termios;

struct history
{
	char entry[HISTORY_MAX_ITEMS][LINE_MAX_LENGTH];
	int head;
	int tail;
	int pos;
} history;

struct line {
	char *buf;		/* Edited line buffer		*/
	size_t buflen;		/* Edited line buffer size	*/
	const char *prompt;	/* Prompt to display		*/
	size_t promptlen;	/* Prompt length		*/
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
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

int term_column_count()
{
	struct winsize ws;
	if (ioctl(1, TIOCGWINSZ, &ws) == -1)
		return 80;

	return ws.ws_col;
}

void buf_append(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->buf, ab->len + len);

	if (new == NULL)
		return;

	memcpy(new + ab->len, s, len);
	ab->buf = new;
	ab->len += len;
}

void buf_free(struct abuf *ab)
{
	free(ab->buf);
}

void history_init()
{
	history.head = 0;
	history.tail = 0;
	history.pos  = 0;
}

void history_push(const char *item)
{
	// Is history full?
	if (((history.head + 1) % HISTORY_MAX_ITEMS) == history.tail)
		history.tail = (history.tail + 1) % HISTORY_MAX_ITEMS;

	strcpy(history.entry[history.head], item);
	history.head = (history.head + 1) % HISTORY_MAX_ITEMS;
	history.pos = history.head;

	// Clean current entry in the head
	memset(history.entry[history.head], 0, sizeof(history.entry[history.head]));
}

/* FIXME: This is broken I think
char *history_get(int index)
{
	printf("\nhistory cur_pos: %i\n", index);
        int pos = history.pos - index;
	printf("history_get(%i)\n", pos);
        if (pos < 0)
		return history.entry[HISTORY_MAX_ITEMS + pos];

        return history.entry[pos];
}
*/

char *history_prev()
{
	// History is empty
	if (history.head == history.tail)
		return history.entry[history.head];

	// Previous is tail?
	if (history.pos == history.tail)
		return history.entry[history.tail];

	int pos = history.pos - 1;

	if (pos < 0)
		pos = HISTORY_MAX_ITEMS + pos;

	history.pos = pos;
	return history.entry[history.pos];
}

char *history_next()
{
	// History is empty
	if (history.head == history.tail)
		return history.entry[history.head];

	// Next is head?
	if (history.pos == history.head)
		return history.entry[history.head];

	int pos = (history.pos + 1) % HISTORY_MAX_ITEMS;

	history.pos = pos;
	return history.entry[history.pos];
}

#if 1
void history_print()
{
	printf("\n");

	for (int i = 0; i < HISTORY_MAX_ITEMS; i++) {
		printf("\r%i: %s", i, history.entry[i]);

		if (history.head == i)
			printf(" <-- HEAD ");

		if (history.tail == i)
			printf(" <-- TAIL ");

		if (history.pos == i)
			printf(" <-- POS ");

		printf("\n");
	}

	printf("History position: %i", history.pos);
	printf("\n");
}
#endif

void refresh_line(struct line *l)
{
	char cursor[64];
	struct abuf ab = ABUF_INIT;

	/* Cursor to left edge */
	snprintf(cursor, 64, "\r");
	buf_append(&ab, cursor, strlen(cursor));

	/* Write prompt and buffer content */
	buf_append(&ab, l->prompt, l->promptlen);
	buf_append(&ab, l->buf, l->len);

	/* Erase to right */
	snprintf(cursor, 64, "\x1b[0K");
	buf_append(&ab, cursor, strlen(cursor));

	/* Move cursor to original position */
	snprintf(cursor, 64, "\r\x1b[%dC", (int)(l->pos + l->promptlen));
	buf_append(&ab, cursor, strlen(cursor));

	write(STDOUT_FILENO, ab.buf, ab.len);

	buf_free(&ab);
}

void line_clear(struct line *l)
{
	memset(l->buf, '\0', l->buflen);
	l->len = 0;
	l->pos = 0;
}

void line_edit(struct line *l, char c)
{
	if (l->len >= l->buflen)
		return;

	if (l->len != l->pos)
		memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);

	l->buf[l->pos] = c;
	l->len++;
	l->pos++;
	l->buf[l->len] = '\0';
}

void line_set(struct line *l, char *string)
{
	strcpy(l->buf, string);
	l->len = strlen(string);
	l->pos = l->len;
}

void line_backspace(struct line *l)
{
	if (l->pos > 0 && l->len > 0) {
		memmove(l->buf + l->pos - 1, l->buf + l->pos, l->len - l->pos);
		l->len--;
		l->pos--;
		l->buf[l->len] = '\0';
		refresh_line(l);
	}
}

/* TODO: Rename to x_getc */
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
#if 1
		case CTRL('r'):
			history_print();
			break;
#endif

		case CTRL('d'):
			exit(0);
			break;

		case CTRL('h'):
			if (l->pos > 0)
				l->pos--;
			break;

		case CTRL('l'):
			if (l->pos != l->len)
				l->pos++;
			break;

		case CTRL('k'):
			// If line isnt empty, preserve it
			if (history.pos == history.head && l->len > 0)
				strcpy(history.entry[history.head], l->buf);

			line_set(l, history_prev());
			break;

		case CTRL('j'):
			line_set(l, history_next());
			break;

		case 127: /* Backspace */
			line_backspace(l);
			break;

		case 13: /* Enter */
			history_push(l->buf);
			printf("\n");
			line_clear(l);
			break;

		default:
			if (isprint(c))
				line_edit(l, c);
			break;
	}


	refresh_line(l);
}

int main()
{
	tty_raw_mode();
	history_init();

	struct line l;

	char buffer[LINE_MAX_LENGTH];

	l.buf = buffer;
	l.buflen = LINE_MAX_LENGTH;

	l.prompt = "$ ";
	l.promptlen = strlen("$ ");

	l.pos = 0;
	l.len = 0;

	refresh_line(&l);

	while (1) {
		process_key(&l);
	}

	return 0;
}
