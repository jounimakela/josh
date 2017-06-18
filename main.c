#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

#define ABUF_INIT { NULL, 0 }
#define MIN(a,b) (((a)<(b))?(a):(b))

#define HISTORY_MAX_ITEMS       1024
#define LINE_MAX_LENGTH         256

struct termios orig_termios;

struct history {
	char entry[HISTORY_MAX_ITEMS][LINE_MAX_LENGTH];
	int head;
	int tail;
	int pos;
} history;

struct line {
	char *buf;              /* Edited line buffer		*/
	size_t buflen;          /* Edited line buffer size	*/
	const char *prompt;     /* Prompt to display		*/
	size_t promptlen;       /* Prompt length		*/
	size_t pos;             /* Current cursor position	*/
	size_t len;             /* Current edited line length	*/

	size_t old_pos;
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
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
		die("tcsetattr");
	}
}

void tty_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
		die("tcgetattr");
	}

	atexit(tty_restore);

	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		die("tcsetattr");
	}
}

int term_column_count()
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
		return 80;
	}

	return ws.ws_col;
}

void buf_append(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->buf, ab->len + len);

	if (new == NULL) {
		return;
	}

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
	/* Is history full? */
	if (((history.head + 1) % HISTORY_MAX_ITEMS) == history.tail) {
		history.tail = (history.tail + 1) % HISTORY_MAX_ITEMS;
	}

	strcpy(history.entry[history.head], item);
	history.head = (history.head + 1) % HISTORY_MAX_ITEMS;
	history.pos = history.head;

	/* Clean current entry in the head */
	memset(history.entry[history.head],
	       0,
	       sizeof(history.entry[history.head]));
}

char *history_prev()
{
	/* History is empty */
	if (history.head == history.tail) {
		return history.entry[history.head];
	}

	/* Previous is tail? */
	if (history.pos == history.tail) {
		return history.entry[history.tail];
	}

	int pos = history.pos - 1;

	if (pos < 0) {
		pos = HISTORY_MAX_ITEMS + pos;
	}

	history.pos = pos;
	return history.entry[history.pos];
}

char *history_next()
{
	/* History is empty */
	if (history.head == history.tail) {
		return history.entry[history.head];
	}

	/* Next is head */
	if (history.pos == history.head) {
		return history.entry[history.head];
	}

	int pos = (history.pos + 1) % HISTORY_MAX_ITEMS;

	history.pos = pos;
	return history.entry[history.pos];
}

#if 0
void refresh_line(struct line *l)
{
	char cursor[64];
	struct abuf ab = ABUF_INIT;
	char *buf  = l->buf;
	size_t len = l->len;
	size_t pos = l->pos;

	while((l->promptlen+pos) >= l->cols) {
		buf++;
		len--;
		pos--;
	}

	while (l->promptlen+len > l->cols) {
		len--;
	}:

	/* */


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
#else
void refresh_line(struct line *l)
{
	struct abuf ab = ABUF_INIT;
	char tmp[LINE_MAX_LENGTH];
	/* TODO: Can this be done some other way? */
	int cols = term_column_count() - 1;

	/* Rows used by current buf */
	int rows = (l->promptlen + l->len) / cols;

	/* Relative cursor position */
	int relative_row = (l->promptlen + l->pos) / cols;
	int relative_col = (l->promptlen + l->pos) % cols;

	for (int row = relative_row; row <= rows; row++) {
		/* Cursor to left edge */
		snprintf(tmp, LINE_MAX_LENGTH, "\r");
		buf_append(&ab, tmp, strlen(tmp));

		/* Write the prompt and current fragment */
		/* Is the shortcut !row? Can we check this outside of the loop? */
		if (row == 0) {
			buf_append(&ab, l->prompt, l->promptlen);
			int len = MIN(cols - l->promptlen, l->len);
			buf_append(&ab, l->buf, len);
		} else {
			int len = MIN(cols, (int)l->len + (int)l->promptlen - (row * cols));
			buf_append(&ab, l->buf + (cols * row - l->promptlen), len);
		}

		/* If we are at the very end of the screen with our prompt, we need to
		 * emit a newline and move the prompt to the first column.
		 */
		if (row == 0 && (l->len + l->promptlen + 1) == cols) {
			snprintf(tmp, LINE_MAX_LENGTH, "\n\r");
			buf_append(&ab, tmp, strlen(tmp));
		} else {
			/* Erase to right if last */
			snprintf(tmp, LINE_MAX_LENGTH, "\x1b[0K");
			buf_append(&ab, tmp, strlen(tmp));
		}
	}

	/* Move cursor to original position */
	snprintf(tmp, 64, "\r\x1b[%dC", relative_col);
	buf_append(&ab, tmp, strlen(tmp));

	write(STDOUT_FILENO, ab.buf, ab.len);

	buf_free(&ab);
}
#endif

void line_clear(struct line *l)
{
	memset(l->buf, '\0', l->buflen);
	l->len = 0;
	l->pos = 0;
}

void line_edit(struct line *l, char c)
{
	if (l->len >= l->buflen - 1) {
		return;
	}

	if (l->len != l->pos) {
		memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
	}

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
	while (read(STDIN_FILENO, &c, 1) != 1) {
		;
	}
	return c;
}

void process_key(struct line *l)
{
	char c = read_key();

	switch (c) {
	case CTRL('d'):
		exit(0);
		break;

	case CTRL('h'):
		if (l->pos > 0) {
			l->pos--;
		}
		break;

	case CTRL('l'):
		if (l->pos != l->len) {
			l->pos++;
		}
		break;

	case CTRL('k'):
		if (history.pos == history.head && l->len > 0) {
			strcpy(history.entry[history.head], l->buf);
		}

		line_set(l, history_prev());
		break;

	case CTRL('j'):
		line_set(l, history_next());
		break;

	case 127:         /* Backspace */
		line_backspace(l);
		break;

	case 13:         /* Enter */
		history_push(l->buf);
		printf("\n");
		line_clear(l);
		break;

	default:
		if (isprint(c)) {
			line_edit(l, c);
		}
		break;
	}

	refresh_line(l);
}

#if 1
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

	l.old_pos = l.pos = 0;
	l.len = 0;

	refresh_line(&l);

	while (1) {
		process_key(&l);
	}

	return 0;
}
#else
int main()
{
	tty_raw_mode();

	char buffer[LINE_MAX_LENGTH] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean ante lectus, feugiat eget elit sit amet, suscipit feugiat purus. Mauris in lacus dui. Morbi massa sapien,-luctus-non-lectus-sed, vehicula vulputate velit.";

	struct line l;
	l.buf = buffer;
	l.buflen = LINE_MAX_LENGTH;
	l.prompt = "$ ";
	l.promptlen = strlen("$ ");
	l.old_pos = l.pos = 0;
	l.len = strlen(l.buf);

	refresh_line(&l);

	return 0;
}
#endif
