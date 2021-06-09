




/* File:	 kilo.c																*/
/* Author:   Aidan U. Gerbofsky													*/
/* Date:     April 30, 2021														*/
/* ====[DESCRIPTION]=========================================================== */
/* This program is a simple text-editor based on a tutorial series which 		*/
/* teaches how to implement a text-editor based on the kilo editor by			*/
/* ============================================================================ */





/* ====[INCLUDES]========================================================================================================= */




#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


/* Must include this for the sake of our poor compiler... */
#include <stdio.h>
/* Standard C Library file that allows functions to take an indefinate number of arguments. */
#include <stdarg.h>
/* Standard C Library file that gives us access to useful 'idioms' such as macros. */
#include <ctype.h>
/* Standard C Library file that provides functions that manage disk reads and writes. */
#include <fcntl.h>
/* Standard C Library file that will be used for error checking and memory management. */
#include <stdlib.h>
/* Standard C Library file that provides functions for string manipulation such as memcpy(). */
#include <string.h>
/* Library that provides additional I/O primatives.*/
#include <sys/ioctl.h>
/* Library file that adds additional functionality to types. */
#include <sys/types.h>
/* Standard C Library file that will provide more effective error handling functions. */
#include <errno.h>
/* POSIC Library that provides functions to control terminal I/O and signals.   */
#include <termios.h>
/* Standard C Library file that provides formatted time related functions, constants, etc. */
#include <time.h>
/* POSIX Library where we can access I/O primative functions such as read, write, etc. */
#include <unistd.h>





/* ====[DEFINES]========================================================================================================== */





#define KILO_VERSION 	"0.0.1"
#define KILO_TAB_STOP	8
#define CTRL_KEY(k)		((k) & 0x1f)




/* Enumeration of the arrow keys. */
enum editorKey
{
	/* Special characters. */
	BACKSPACE = 127,

	/* Begining the enumeration at 1000 ensures that there will be no value conflicts. */
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};





/* ====[DATA]============================================================================================================= */






/* Structure that defines what a row of data is. */
typedef struct erow
{
	int size;
	int rsize;

	char *chars;
	char *render;
}erow;





/* Structure that will be used as a template for global state */
struct editorConfig
{
	/* cx and cy will be used to control the cursor position. */
	int cx, cy;
	/* rx represents the render's temp. horizontal position.*/
	int rx;
	/* rowoff is the "row offset". */
	int rowoff;
	/* coloff is the "column offset. "*/
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	
	erow *row;
	/* This pointer is where the filename will be stored. */
	char *filename;
	/* These pointers will be responsible for storeing messages to be displayed on the */
	/* Status bar, along with the current system time.								   */
	char statusmsg[80];
	time_t statusmsg_time;

	struct termios orig_termios;
};

/* Instantize our editor configuration structure. */
struct editorConfig E;





/* ====[PROTOTYPES]======================================================================================================= */
void editorSetStatusMessage(const char *fmt, ...);




/* ====[DATA]============================================================================================================= */





/* Function that handles user application termination. */
void terminate(const char *s)
{
	/* Clear the screen upon closing the application. */
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}





/* Function to disable raw text input.*/
void disableRawMode()
{
	/* Sets terminal attributes to that of the initial terminal */
	/* configuration, stored in our "orig_termios" structure.   */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		terminate("tcsetattr 1");
}





/* Function to enable raw text input. */
void enableRawMode()
{	
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		terminate("tcgetattr");
	atexit(disableRawMode);
		
	/* Instantize the termios structure as "raw". */
	struct termios raw = E.orig_termios;

	/* Read the current file attributes. */
	tcgetattr(STDIN_FILENO, &raw);

	/* ICRNL disables 'Ctrl-M' IXON Disables 'Ctrl-S' and   */
    /* 'Ctrl-Q' signals. The other signal flags are included */
	/* for similar reasons.									 */
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

	/* Disable terminal output processing. We are not using */
	/* teletypes... even though that would be awesome.		*/
	raw.c_lflag &= ~(OPOST);
	raw.c_lflag != (CS8);

	/* Turn off terminal input echoing. The ICANON attribute */
	/* allows us to enter input on a byte-by-byte basis		 */
	/* rather than on a line-by-line basis.	ISIG allows us   */
	/* to control terminal input signals. We will be using   */
	/* it to disable 'Ctrl-C' and 'Ctrl-Z' signals.	IEXTEN   */
	/* will allow us to disable 'Ctrl-V'.					 */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	/* Controls user input timeout. */
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		terminate("tcsetattr 3");
}





/* Function responsible for handling keyboard input. */
int editorReadKey()
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN)
			terminate("read");
	}

	/* This IF-ELSE structure essentially aliases the arrow keys as WASD keys. */
	/* NOTE: This is entirely temporary, as anyone with sense could imagine... */
	if (c == '\x1b')
	{
		char seq[3];

		if (read(STDIN_FILENO, &seq[0],1 ) != 1)
			return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return '\x1b';
		
		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				
				if (seq[2] == '~')
				{
					switch (seq[1]) 
					{
						case '1':
							return HOME_KEY;
						case '3':
							return DEL_KEY;
						case '4':
							return END_KEY;
						case '5':
							return PAGE_UP;
						case '6':
							return PAGE_UP;
						case '7':
							return HOME_KEY;
						case '8':
							return END_KEY;
					}
				}
			}
			else
			{ 
				switch (seq[1])
				{
					case 'A':
						return ARROW_UP;
					case 'B':
						return ARROW_DOWN;
					case 'C':
						return ARROW_RIGHT;
					case 'D':
						return ARROW_LEFT;
					case 'H':
						return HOME_KEY;
					case 'F':
						return END_KEY;
				}
			}
		}

		else if (seq[0] == 'O')
		{
			switch(seq[1])
			{
				case 'H':
					return HOME_KEY;
				case 'F':
					return END_KEY;
			}
		}

		return '\x1b';
	}
	
	else
		return c;
}





/* Function responsible for configuring/getting the cursor position. */
int getCursorPosition(int *rows, int *cols)
{
	/* Buffer for response parsing. */
	char buf[32];
	unsigned int i = 0;

	/* Here the 6 asks for the cursor position while "n" allows us to */
	/* querry the terminal emulator for status information.			  */
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	printf("\r\n");

	while (i < sizeof(buf) -1)
	{
		if (read (STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}

	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[')
		return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
		return -1;

	return 0;
}





/* Function resposible for controlling window size parameters. */
int getWindowSize(int *rows, int *cols)
{
	struct winsize ws;
	
	/* There is no guarentee that ioctl() will always work so we need to provide */
	/* a fall back mechanisim. This will be done using the manual x-primatives   */
	/* for controlling cursor position. You can see this before the "C" and "B". */
	/* Additionally, 999 represents the very bottom right of the screen. Also,   */
	/* the 1 infront of the ioctl function is temporary. allowing us to test our */
	/* fall-back mechanism.														 */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
		{
			return getCursorPosition(rows, cols);
		}
	}

	else
	{
		*rows = ws.ws_row;
		*cols = ws.ws_col;
		return 0;
	}
}





/* Function responsible for translating the text relative to the cursor position to */
/* the frame relative to the render target.											*/
int editorRowCxToRx(erow *row, int cx)
{
	int rx = 0;
	int j;

	for (j = 0; j < cx; j++)
	{
		if (row->chars[j] == '\t')
			rx += ((KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP));
		
		rx++;
	}
	return rx;
}





/* Function that is responsible for rendering the contents of a row. */
void editorUpdateRow(erow *row)
{
	int j;
	int tabs = 0;
	/* idx will contain the number of characters we will be copying into */
	/* row->render.														 */
	int idx = 0;

	/* Calculate the amount of tabs on the current line.*/
	for (j = 0; j < row->size; j++)
		if (row->chars[j] == '\t')
			tabs++;

	free(row->render);
	row->render = malloc(row->size + ((tabs * (KILO_TAB_STOP - 1)) + 1));
	
	/* Now render the tabs detected as a series of spaces. */
	for (j = 0; j < row->size; j++)
		if (row->chars[j] == '\t')
		{
			row->render[idx++] = ' ';

			while ((idx % KILO_TAB_STOP) != 0)
				row->render[idx++] = ' ';
		}

		else
			row->render[idx++] = row->chars[j];

	row->render[idx] = '\0';
	row->rsize = idx;
}





/* Function similar to abAppend() in that it is responsible for */
/* as the name implies, appending a row of text. With that,     */
/* That means that this function must also be incharge of       */
/* memory resource allocation.									*/
void editorAppendRow(char *s, size_t len)
{
	/* Reallocate the memory of the row to enough space for appending. */
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	/* Here, "at" will represent the reallocation target row. */
	int at = E.numrows;
	
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);

	/* Transfer the old + new data into the newly reallocated row. */	
	memcpy(E.row[at].chars, s, len);
	
	/* Append an EOF at the end of the row, and move on to the next. */
	E.row[at].chars[len] = '\0';
	
	E.row[at].rsize = 0;
	E.row[at].render = NULL;

	editorUpdateRow(&E.row[at]);

	E.numrows++;
}




/* Function that handles row insert. */
void editorRowInsertChar(erow *row, int at, int c)
{
	/* Check the position/bounds of at relative to the length of the row. */
	if (at < 0 || at > row->size)
		at = row->size;

	/* Reallocate the memory resources for the evaluated row of characters. */
	/* Here we incremented by two so as to make room for the NULL byte.     */
	row->chars = realloc(row->chars, row->size + 2);

	/* Shift the positioning of the chars in the row to the right by 1. */
	memmove(&row->chars[at + 1], &row->chars[at], row->size - (at + 1));

	/* Increase the length of the row. */
	row->size++;
	/* Set the at_th index of the chars buffer to the value of c/the char */
	/* to be inserted into the row buffer at the at_th position.          */
	row->chars[at] = c;
	editorUpdateRow(row);
}

/* Function responsible for fetching the keyboard presses to be processed. */
void editorInsertChar(int c)
{
	if (E.cy == E.numrows)
	{
		editorAppendRow("", 0);
	}

	/* Call the routine responsible for processing characters to be inserted. */
	editorRowInsertChar(&E.row[E.cy], E.cx, c);
	/* Move the cursor position to the next place.in the row. Move the cursor's */
	/* X-position forward by 1 place.                                           */
	E.cx++;
}

/* Function responsible for handling writes to system disk/mass-storage-devices. */
char *editorRowsToString(int *buflen)
{
	int totlen = 0;
	int j;

	/* Calculate the size of the file. */
	for (j = 0; j < E.numrows; j++)
		totlen += E.row[j].size + 1;
	
	/* The length of the file buffer is equal to the calculated "total" length. */
	*buflen = totlen;

	/* Allocate the space for the file-to-storage buffer. */
	char *buf = malloc(totlen);
	char *p = buf;

	for (j = 0; j < E.numrows; j++)
	{
		memcpy(p, E.row[j].chars, E.row[j].size);

		/* Advance the storage access buffer forward to the next row. */
		p += E.row[j].size;
		*p = '\n';
		p++;
	}

	/* The expected caller is free(). */
	return buf;
}




/* Function responsible for handling file I/O. */
void editorOpen(char *filename)
{
	/* Make a copy of the string containing the file's name. */
	free(E.filename);
	E.filename = strdup(filename);

	/* Open the file specified and check incase there is none. */
	FILE *fp = fopen(filename, "r");
	if (!fp)
		terminate("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	while ( (linelen = getline(&line, &linecap, fp)) != -1)
	{
		while (linelen > 0 && (line[linelen - 1] == '\n' ||
							   line[linelen - 1] == '\r'))
			linelen--;

		editorAppendRow(line, linelen);

	}

	free(line);
	fclose(fp);
}




/* Function that is responsible for writing to disk. */
void editorSave()
{
	/* Check if the file being written is a newfile. */
	if (E.filename == NULL) 
		return;

	int len;

	/* Our write buffer size will be equal to the value returned by... */
	char *buf = editorRowsToString(&len);

	/* Open a file. If it already exists, then open it for writes. If it does not, */
	/* Create a new file, under thant file name, with read/write permission.       */
	int fd = open(E.filename, O_RDWR | O_CREAT, 0644);

	/* Error checking before final write to disk. */
	if (fd != -1)
	{
		if(ftruncate(fd, len) != -1)
		{
			if(write(fd, buf, len) == len)
			{
				close(fd);
				free(buf);

				editorSetStatusMessage("%d bytes written to disk", len);

				return;
			}	
		}
		close(fd);
	}

	free(buf);

	editorSetStatusMessage("Can't save ! I/O error: %s", strerror(errno));
}




/* structure that defines our append buffer. Creates a dynamic/mutable string type. */
struct abuf
{
	char *b;
	int len;
};

/* This defines works as a contructor would in C++. The constant definition defines */
/* what exactly an empty buffer is.													*/
#define ABUF_INIT {NULL, 0}





/* Function that defines append operations on the append buffer/"abuf". */
void abAppend(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
		return;

	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}





/* Function that defines how to free the memory allocated to an instance of the abuf. */
void abFree(struct abuf *ab)
{
	free(ab->b);
}





/* Function responsible for handling the vertical scroll of the editor. */
void editorScroll()
{
	E.rx = 0;

	if (E.cy < E.numrows)
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

	if (E.cy < E.rowoff)
		E.rowoff = E.cy;

	if (E.cy >= (E.rowoff + E.screenrows))
		E.rowoff = (E.cy - E.screenrows + 1);

	if (E.rx < E.coloff)
		E.coloff = E.rx;

	if (E.rx >= (E.coloff + E.screencols))
		E.coloff = (E.rx - (E.screencols + 1));	
}





/* Function that draws rows of tildes, like VIM does. */
void editorDrawRows(struct abuf *ab)
{
	int y;
	for (y = 0; y < E.screenrows; y++)
	{
		int filerow = (y + E.rowoff);

		if (filerow >= E.numrows) 
		{
			if (E.numrows == 0 && y == (E.screenrows / 3))
			{
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome), 
						"kilo editor -- version %s", KILO_VERSION);
				if (welcomelen > E.screencols)
					welcomelen = E.screencols;

				/* Now we handle the condition if the terminal window is too small to */
				/* Display the entire contents of the welcome message, by truncating  */
				/* the message into a series of strings, thus making the message      */
				/* viewable...														  */
				int padding = (E.screencols - welcomelen) / 2;

				if (padding)
				{
					abAppend(ab, "~", 1);
					padding--;
				}

				while (padding--) 
					abAppend(ab, " ", 1);

				abAppend(ab, welcome, welcomelen);
			}	
		
		else
			abAppend(ab, "~", 1);
		}
		
		else
		{
			int len = (E.row[filerow].rsize - E.coloff);
			
			if (len < 0)
				len = 0;
			
			if (len > E.screencols) 
				len = E.screencols;
			
			abAppend(ab, &E.row[filerow].render[E.coloff], len);
		}
		
		/* Allows the terminal to clear the line that is outside of the render, as */
		/* the user scrolls up and down a file. 								   */
		abAppend(ab, "\x1b[K", 3);

		abAppend(ab, "\r\n", 2);
	}
}





/* Function responsible for drawing the status bar at the bottom of the editor. */
void editorDrawStatusBar(struct abuf *ab)
{
	/* "\x1b[7m sets the text to inverted colour mode." */
	abAppend(ab, "\x1b[7m", 4);
	/* The char buffer "status" will be used to store the file's name.     */
	/* The char buffer "rstatus" will be used to store the line number, at */
	/* the right side of the status bar.								   */
	char status[80];
	char rstatus[80];
	
	/* Configure the length of the string, while checking if there is a specified name. */
	/* If there is no name specified, len is equal to the length of "[No Name]"         */
	int len = snprintf(status, sizeof(status), "%.20s - %d lines",
				E.filename ? E.filename : "[No Name]", E.numrows);

	/* The length of the string stored at the right side of the status bar is equal to the */
	/* the length of the Cursor's y position and the Current row\line number.              */
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);

	/* Check the bounds of the string. If it satisfies the bounds, append the file's name */
	/* to the status bar.																  */
	if (len > E.screenrows)
		len = E.screenrows;

	abAppend(ab, status, len);

	/* Draw the status bar such that the current position is within the bounds of len. */
	while (len < E.screencols)
	{
		/* This IF-ELSE structure calculates the centered positioning of the string to be  */
		/* displayed, relative to its designated area on the status bar. If it is centered */
		/* the string is appended to the status bar.									   */
		if ((E.screencols - len) == rlen)
		{
			abAppend(ab, rstatus, rlen);
			break;
		}

		else
		{
			abAppend(ab, " ", 1);
			len++;
		}
	}

	/* This append function uses the "\x1b[m" which turns off inverted text mode. */
	abAppend(ab, "\x1b[m", 3);
	abAppend(ab, "\r\n", 2);
}




/* Function responsible for generating/displaying the message bar. */
void editorDrawMessageBar(struct abuf *ab)
{
	/* No cursor on the message bar, thanks... */
	abAppend(ab, "\x1b[K", 3);

	/* msglen is equal to the length of the status message. */
	int msglen = strlen(E.statusmsg);

	/* Check the bounds of the message. */
	if (msglen > E.screencols)
		msglen = E.screencols;

	/* Center and append the status message and the time/date onto the message bar.*/
	if (msglen && time(NULL) - E.statusmsg_time < 5)
		abAppend(ab, E.statusmsg, msglen);
}




/* Function that controlls the rendering and updating of screen content. */
void editorRefreshScreen()
{
	/* Call the editorScroll function to see if the renderer must move the */
	/* verticle frame up or down by 1 position. 						   */
	editorScroll();

	struct abuf ab = ABUF_INIT;

	/* Gets rid of that annoying flickering. NOTE: "l" and "h" represent  */
	/* "set mode" and "mode reset". The argument "?25" controlls whether  */
	/* the cursor is shown or hidden.									  */
	abAppend(&ab, "\x1b[?25h", 6);
	/* Enables repositioning of the cursor. */
	abAppend(&ab, "\x1b[H", 3);

	/* Draw the background "decorations" and then reposition the cursor. */
	editorDrawRows(&ab);
	/* Call the routine that draws the status bar. */
	editorDrawStatusBar(&ab);
	/* Call the routine that is responsible for displaying the message bar. */
	editorDrawMessageBar(&ab);

	/* This segment of code will control the display of the cursors at n-position, */
	/* As the program iterates, and the values of the cursor's x and y position    */
	/* are updated. 															   */
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);


	/* Reposition the cursor, and retire the current instance of the abuf. */
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}





/* Function responsible for configuring the status bar message and time/date. */
void editorSetStatusMessage(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
	
	va_end(ap);

	E.statusmsg_time = time(NULL);
}




/* This function will be responsible for providing cursor movement. */
void editorMoveCursor(int key)
{
	/* This ternary checks if the cursor is on an actuall line. If it is, then   */
	/* the row variable will point to the erow that the cursor is on. Else check */
	/* whether E.cx is to the left of the end of that line before we allow the   */
	/* cursor to move to the right. 											 */
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch (key)
	{
	case ARROW_LEFT:
		if (E.cx != 0)
			E.cx--;

		/* Allows the user to move to the end of the previous line by pressing */
		/* ARROW_LEFT such that ARROW_LEFT is pressed when E.cx = 0.		   */
		else if (E.cy > 0)
		{
			E.cy--;
			E.cx = E.row[E.cy].size;	
		}

		break;
	
	case ARROW_RIGHT:
		if (row && (E.cx < row->size))
			E.cx++;

		/* Allows the user to move to the end of the next line by pressing     */
		/* ARROW_RIGHT such that ARROW_RIGHT is pressed when E.cx = the length */
		/* of the row; when the cursor is at the end of the current row.       */
		else if (row && (E.cx == row->size))
		{
			E.cy++;
			E.cx = 0;
		}

		break;
	
	case ARROW_UP:
		if (E.cy != 0)
			E.cy--;
		break;
	
	case ARROW_DOWN:
		if (E.cy < E.numrows)
			E.cy++;
		break;
	}

	/* This code ensures that the cursor is snapped to the end of a line. */
	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;

	if (E.cx > rowlen)
	{
		E.cx = rowlen;
	}
}





/* Function that handles editor input processing. */
void editorProcessKeypress()
{
	int c = editorReadKey();

	switch (c)
	{
		case '\r':
		/* 	 TODO	*/
		break;

		/* Code for the "Quit" key-binding. */
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);

			exit(0);
			break;

		/* Code for the "Save file" key-binding. */
		case CTRL_KEY('s'):
			editorSave();
			break;

		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			if (E.cy < E.numrows)
				E.cx = E.row[E.cy].size;

			break;

		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			/* 	 TODO	*/
			break;

		/* *NOTE: THERE IS A BUG HERE* */
		case PAGE_UP:
		case PAGE_DOWN:
			{
				if (c == PAGE_UP)
					E.cy = E.rowoff;

				else if (c == PAGE_DOWN)
				{
					E.cy = (E.rowoff + E.screenrows - 1);

					if (E.cy > E.numrows)
						E.cy = E.numrows;
				}

				int times = E.screenrows;

				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}

			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;

		case CTRL_KEY('l'):
		case 'x1b':
			break;

		/* The default case will always be to insert characters. */
		default:
			editorInsertChar(c);
			break;
	}
}





/* Function responsible for initializing our editor. */
void initEditor()
{
	/* Set the initial values of the cursor's x and y position. */
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1)
		terminate("getWindowSize");

	/* Here we decrement by 1 so that editorDrawRows( doesn't try to draw a line at */
	/* the very bottom of the rendered (visible text) buffer. This is so that we    */
	/* will have enough space to include our status bar.							*/

	/* We added another to make it 2. This makes it so that a new line will be      */
	/* Displayed after the status bar is finished being rendered/displayed. This is */
	/* the space needed for the message bar to be displayed.                        */
	E.screenrows -= 2;
}




/* UNDERRATED LOL. No but seriously... this is the main function if you couldn't see already. */
int main(int argc, char *argv[])
{
	/* Enable raw-text mode at the begining of the application. */
	enableRawMode();
	/* Call the editor initialization function. */
	initEditor();
	/* File I/O function. */
	if (argc >= 2)
		editorOpen(argv[1]);

	/* Set the initial status message.*/
	editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

	/* The main program loop will iterate indefinately, until read() returns 0, */
	/* OR until the user enters the character 'Ctrl-q'.							*/
	while (1)
	{	
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}
/* ====[END-OF-FILE]====================================================================================================== */