/* Include directives */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


/* defines */
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

enum editorKey
{
    BACKSPACE = 127,
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

enum editorHighlight // highlight types
{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};


/* data */
struct editorSyntax
{
    char *filetype;
    char **filematch, **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start, *multiline_comment_end;
    int flags;
};

typedef struct erow // struct for individual line
{
    int idx, size, rsize; // index, row size and rendered row size
    char *chars, *render; // row characters
    unsigned char *hl; // for highlighting different types of characters
    int hl_open_comment; // highlight open comment in row
} erow;

struct editorConfig
{
    int cx, cy, rx; // x, y position of cursor
    int rowoff, coloff; // offset of rows and columns
    int screenrows, screencols; // # of rows and columns shown on screen
    int numrows; // number of rows
    int dirty; // variable to keep track of modified buffer
    erow *row; 
    char *filename; // filename of current file
    char statusmsg[80]; // array to hold status msg for user
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios; // restore terminal at exit
};

struct editorConfig E;


/* filetypes */
// file type extensions
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };

// keywords and types for highlighting
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL
};

// array for syntax highlights
struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions, // uses file extention types
        C_HL_keywords, // uses keywords 
        "//", "/*", "*/", // uses comment delimiters
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))


/* prototypes */
// function declarations
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));


/* terminal */
void die(const char *s)
{
    // clear the screen on exit
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s); // error handling
    exit(1); // exit program
}

// disable raw mode
void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

// enable raw mode
void enableRawMode() 
{
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode); // disable raw mode with exiting

    struct termios raw = E.orig_termios;

    // disable break, CR to NL, parity, strip and start-stop output flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // disable post processing flag
    raw.c_oflag &= ~(OPOST);
    // disable 8-bit flag
    raw.c_cflag |= (CS8);
    // disable echo, canonical, extention, signal character flags
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0; // set minimum # of bytes
    raw.c_cc[VTIME] = 1; // set max amount of time to wait before read() returns

    //enable raw mode
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

// function that reads in keys
int editorReadKey()
{
    // variable declarations
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if((nread == -1 && errno != EAGAIN))
            die("read");
    }

    // check to see if the character 'c' is an escape character
    if(c == '\x1b')
    {
        // small character array declaration
        char seq[3];
        // check for the conditions and return the escape character
        if(read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        /* check if the character sequence starts with '['
        *  this character is returned as a sequence after the escape character */
        if(seq[0] == '[')
        {
            // check for the range of the 2nd sequence character
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                // check the last character of the sequence
                if(read(STDIN_FILENO, &seq[2], 1) != 1)
                    // return escape character
                    return '\x1b';
                // if the previous if statement didn't match, then check for '~'
                if(seq[2] == '~')
                {
                    // switch case using the 2nd character of the sequence (numbers)
                    switch(seq[1])
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
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            }
            else
            {
                // switch case using the 2nd character of the sequence (characters)
                switch(seq[1])
                {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_LEFT;
                    case 'D':
                        return ARROW_RIGHT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O')
        {
            // switch case for home and end key
            switch (seq[1])
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
    {
        return c;
    }
}

// method to get cursor position
int getCursorPosition(int *rows, int *cols)
{
    // variable declaration/assignment
    char buf[32];
    unsigned int i = 0;

    // get cursor location
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    
    // check if i is in buf
    while(i < sizeof(buf) - 1)
    {
        if(read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if(buf[i] == 'R')
            break;
        i++;
    }

    // set buf[i] to null
    buf[i] = '\0';
    
    // parse the buf array
    if(buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

// function to get the number of columns and rows of the window
int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        // get position of cursor
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        // return cursor position
        return getCursorPosition(rows, cols);
    }
    else
    {
        // set columns and rows to window size columns and rows
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/* syntax highlighting */
// function that returns true if the specific character is a 'separtor character'
int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

// function that highlights the characters in a row
void editorUpdateSyntax(erow *row)
{
    // variable assignments
    int i = 0, prev_sep = 1, in_string = 0;

    // reallocate size
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    // return if no syntax
    if (E.syntax == NULL)
        return;

    // assign syntax to variables
    char **keywords = E.syntax->keywords;
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;
    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
  
    while(i < row->rsize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        // check for singleline comment
        if(scs_len && !in_string && !in_comment)
        {
            if(!strncmp(&row->render[i], scs, scs_len))
            {
                // from '//' to the end of the row is a comment
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        //check for multi line comment(s) 
        if(mcs_len && mce_len && !in_string)
        {
            if(in_comment)
            {
                row->hl[i] = HL_MLCOMMENT;
                if(!strncmp(&row->render[i], mce, mce_len))
                {
                    // highlighting to the end of the multi line comment
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else
                {
                    i++;
                    continue;
                }
            }
            else if(!strncmp(&row->render[i], mcs, mcs_len))
            {
                // start of multiline comment, and starts highlighting
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if(E.syntax->flags & HL_HIGHLIGHT_STRINGS)
        {
            //checking for string
            if(in_string)
            {
                row->hl[i] = HL_STRING;
                if(c == '\\' && i + 1 < row->rsize)
                {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }

                if(c == in_string)
                    in_string = 0;
                prev_sep = 1;
                i++;
                continue;
            }
            else
            {
                //check for single or double quotes for string
                if(c == '"' || c == '\'')
                {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        // handling highlighting numbers
        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS)
        {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                    (c == '.' && prev_hl == HL_NUMBER))
            {
                row->hl[i] = HL_NUMBER;
                prev_sep = 0;
                i++;
                continue;
            }
        }

        //checking for separator character
        if(prev_sep)
        {
            int j;
            // looping through keywords
            for(j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';

                if(kw2)
                    klen--;

                if(!strncmp(&row->render[i], keywords[j], klen) &&
                        is_separator(row->render[i + klen]))
                {
                    // handling keywords
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }

            // keyword was a match since it is not null
            if(keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }

        // passing char c to check if it is a separator character
        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    // check for open comment and apply syntax to next row if necessary
    if(changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
}

//function that maps highlight values to colors
int editorSyntaxToColor(int hl)
{
    // switch case for highlighting, each case is a different color
    switch(hl)
    {
        case HL_NUMBER:
            return 31;
        case HL_KEYWORD2:
            return 32;
        case HL_KEYWORD1:
            return 33;
        case HL_MATCH:
            return 34;
        case HL_STRING:
            return 35;
        case HL_COMMENT: case HL_MLCOMMENT:
            return 36;
        default:
            return 37;
    }
}

//function that matches filename to filematch
void editorSelectSyntaxHighlight()
{
    // set syntax to null
    E.syntax = NULL;

    // return if file name is empty
    if(E.filename == NULL)
        return;

    char *ext = strrchr(E.filename, '.');

    // select the highlighting syntax
    for(unsigned int j = 0; j < HLDB_ENTRIES; j++)
    {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while(s->filematch[i])
        {   
            //check for file match
            int is_ext = (s->filematch[i][0] == '.');
            if((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                    (!is_ext && strstr(E.filename, s->filematch[i])))
            {
                // set syntax to s
                E.syntax = s;
                for(int filerow = 0; filerow < E.numrows; filerow++)
                {
                    editorUpdateSyntax(&E.row[filerow]);
                }

                return;
            }
            i++;
        }
    }
}


/* row operations */
// function to change character index
int editorRowCxToRx(erow *row, int cx)
{
    //variable declaration/initialization
    int j, rx = 0;
    
    // convert character index to render index
    for(j = 0; j < cx; j++)
    {
        if(row->chars[j] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
    }
    return rx;
}

// function to change character index
int editorRowRxToCx(erow *row, int rx)
{
    //variable declaration/initialization
    int cx, cur_rx = 0;

    // convert render index back to character index
    for(cx = 0; cx < row->size; cx++)
    {
        if(row->chars[cx] == '\t')
            cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
        cur_rx++;
        if(cur_rx > rx)
            return cx;
    }
    
    return cx;
}

//function that updates render array when current row changes
void editorUpdateRow(erow *row)
{
    //variable declaration/initialization
    int j, idx = 0, tabs = 0;

    // check for tabs
    for(j = 0; j < row->size; j++)
        if(row->chars[j] == '\t')
            tabs++;
    
    // free and allocate space for rendered row
    free(row->render);
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);
    
    // iterate through row characters
    for(j = 0; j < row->size; j++)
    {
        if(row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }
    // set render[idx] to null and update hightlighting syntax for the row
    row->render[idx] = '\0';
    row->rsize = idx;
    editorUpdateSyntax(row);
}

// function to insert row
void editorInsertRow(int at, char *s, size_t len)
{
    if(at < 0 || at > E.numrows)
        return;
    // reallocate more space and increase row count
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for(int j = at + 1; j <= E.numrows; j++)
        E.row[j].idx++;

    // assign new values to struct variables
    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    //call function and pass row[at]
    editorUpdateRow(&E.row[at]);
    //increment row count and modified buffer
    E.numrows++;
    E.dirty++;
}

// function to free up space
void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

//function to remove row
void editorDelRow(int at)
{
    if(at < 0 || at >= E.numrows)
        return;

    // free row space and delete row, moving the other rows up by 1
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    for(int j = at; j < E.numrows - 1; j++)
        E.row[j].idx--;
    // decrement row count and increment modified buffer
    E.numrows--;
    E.dirty++;
}

//function to insert characters at cursor position
void editorRowInsertChar(erow *row, int at, int c)
{
    // set at equal to row size
    if(at < 0 || at > row->size)
        at = row->size;
    // allocate space for row characters
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    // increase row size
    row->size++;
    // set char[at] to character 'c'
    row->chars[at] = c;
    // call function to update row
    editorUpdateRow(row);
    // modified buffer
    E.dirty++;
}

// function to append a string to current row
void editorRowAppendString(erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    // append string 's' to end of current row
    memcpy(&row->chars[row->size], s, len);
    // update row size
    row->size += len;
    // set null term for end of row
    row->chars[row->size] = '\0';
    //update row
    editorUpdateRow(row);
    //modified buffer
    E.dirty++;
}

// function to delete character in row
void editorRowDelChar(erow *row, int at)
{
    if(at < 0 || at >= row->size)
        return;
    // delete character at [at +1] and decrement row size
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    //update row
    editorUpdateRow(row);
    //modified buffer
    E.dirty++;
}


/* editor operations */
//function to insert character at cursor
void editorInsertChar(int c)
{
    //add empty row
    if(E.cy == E.numrows)
        editorInsertRow(E.numrows, "", 0);

    // insert character in row at cursor position
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    //update cursor x position
    E.cx++;
}

//function to insert new line
void editorInsertNewline()
{
    // add empty row
    if(E.cx == 0)
    {
        editorInsertRow(E.cy, "", 0);
    }
    else
    {
        erow *row = &E.row[E.cy];
        // insert new line in the middle of current row
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        //update row w/ cursor y position
        row = &E.row[E.cy];
        // set row size
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    // update cursor positions
    E.cy++;
    E.cx = 0;
}

//function to delete character
void editorDelChar()
{
    if(E.cy == E.numrows)
        return;
    if(E.cx == 0 && E.cy == 0)
        return;

    erow *row = &E.row[E.cy];

    // checking cursor position
    if(E.cx > 0)
    {
        // delete character left of cursor
        editorRowDelChar(row, E.cx - 1);
        //reposition cursor
        E.cx--;
    }
    else
    {
        // get cursor to move back to previous row
        E.cx = E.row[E.cy - 1].size;
        //append string to previous row
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        //delete row
        editorDelRow(E.cy);
        E.cy--;
    }
}


/* file I/O */
// function to make multiple rows into a string
char *editorRowsToString(int *buflen)
{
    // variable declaration/assignment
    int j, totlen = 0;

    // get the number of bytes for the row
    for(j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    // variable declaration/assignment
    char *buf = malloc(totlen);
    char *p = buf;

    // get the rows into one string
    for(j = 0; j < E.numrows; j++)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}

//function to open editor with file
void editorOpen(char *filename)
{
    // free filename
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

    // open file in read mode
    FILE *fp = fopen(filename, "r");

    // if file not open, then print error 
    if(!fp)
        die("fopen");

    //variable declaration/assignment
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while(linelen > 0 && (line[linelen - 1] == '\n' ||
                line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    //free line
    free(line);
    //close file
    fclose(fp);
    //reset buffer
    E.dirty = 0;
}

//function to save file
void editorSave()
{
    // check if file name exists
    if(E.filename == NULL)
    {
        // get file name from user for saving
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        //abort save if file name not entered
        if(E.filename == NULL)
        {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    //variable declaration/assignment
    int len, fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    char *buf = editorRowsToString(&len);
    
    //check if fd returns success or failure
    if(fd != -1)
    {
        //check if ftruncate() returns an error or not
        if(ftruncate(fd, len) != -1)
        {
            //inform user of how much bytes were saved to disk when no errors return
            if(write(fd, buf, len) == len)
            {
                //close fd and free buffer
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        //close fd
        close(fd);
    }
    //free buffer
    free(buf);
    //print error to user if error returns
    editorSetStatusMessage("Cannot save! I/O error: %s", strerror(errno));
}


/* find */
//function for performing search
void editorFindCallback(char *query, int key)
{
    //variable declaration/assignment
    static int last_match = -1, direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if(saved_hl)
    {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    // search forward or backwards depending on what key the user presses
    if(key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;
        return;
    }
    else if(key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        //if 1, search next
        direction = 1;
    }
    else if(key == ARROW_LEFT || key == ARROW_UP)
    {
        //if -1, search previous
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1)
        direction = 1;

    int current = last_match;

    //search through the rows
    for(int i = 0; i < E.numrows; i++)
    {
        current += direction;

        //searching backwards and forwards for each find
        if(current == -1)
        {
            current = E.numrows - 1;
        }
        else if(current == E.numrows)
        {
            current = 0;
        }

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if(match)
        {
            //set lastmatch equal to current find
            last_match = current;
            //set cursor y position on current
            E.cy = current;
            //set cursor x position to character index
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;
            //highlighting current find
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

//function for search finds
void editorFind()
{
    // save cursor position before performingsearching
    int saved_cx = E.cx, saved_cy = E.cy, saved_coloff = E.coloff, 
            saved_rowoff = E.rowoff;

    //prompt user on how to use search and call function that performs the search
    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)",
            editorFindCallback);
    
    if(query)
    {
        free(query);
    }
    else
    {
        //restore the cursor position and column and row offsets
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}


/* append buffer */
struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

//function to append the buffer
void abAppend(struct abuf *ab, const char *s, int len)
{
    // reallocate space
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL)
        return;

    //append string 's' to buffer in memory
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

//function to free up append buffer
void abFree(struct abuf *ab)
{
    //deallocate space
    free(ab->b);
}


/* output */
/* This function creates spaces for a status bar at the bottom of the text 
    editor, and displays numerous different data for the user to see while 
    editing text.
*/
void editorDrawStatusBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[7m", 4);
    //variable declaration/assignment
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
            E.filename ? E.filename : "[No Name]", E.numrows,
            E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
            E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);

    if(len > E.screencols)
        len = E.screencols;

    abAppend(ab, status, len);

    while(len < E.screencols)
    {
        if(E.screencols - len == rlen)
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
    
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

/* This function checks for see if the cursor is within the window for 
    the editor and allows user to scroll within the editor
*/
void editorScroll()
{
    E.rx = 0;
    if(E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

    if(E.cy < E.rowoff)
        E.rowoff = E.cy;

    if(E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;

    if(E.rx < E.coloff)
        E.coloff = E.rx;

    if(E.rx >= E.coloff + E.screencols)
        E.coloff = E.rx - E.screencols + 1;
}

/* This function draws '~' in rows not part of the file and it handles 
    drawing each row o the buffer of the text that is being edited. It also
    draws the number of rows required to fill the window size.
*/
void editorDrawRows(struct abuf *ab)
{
    for(int y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        if(filerow >= E.numrows)
        {
            if(E.numrows == 0 && y == E.screenrows / 3)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                        "Kilo editor -- version %s", KILO_VERSION);
                if(welcomelen > E.screencols) 
                {
                    welcomelen = E.screencols;   
                }
                int padding = (E.screencols - welcomelen) / 2;
                if(padding)
                {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--)
                {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, welcome, welcomelen);
            }
            else
            {
                abAppend(ab, "~", 1);
            }
        }
        else
        {
            int j, len = E.row[filerow].rsize - E.coloff;
            int current_color = -1;
            if(len < 0)
                len = 0;
            if(len > E.screencols)
                len = E.screencols;

            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];

            for(j = 0; j < len; j++)
            {
                if(iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if(current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }
                else if(hl[j] == HL_NORMAL)
                {
                    if(current_color != -1)
                    {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                }
                else
                {
                    int color = editorSyntaxToColor(hl[j]);
                    if(color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

/* This function displays a message in the status bar, but only does so
    if the message is less than 5 seconds old and if it fits within the width
    of the message bar.
*/
void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    
    if(msglen > E.screencols)
        msglen = E.screencols;
    if(msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

/* This function is designed to clear the users screen and reposition the
    cursor to where it was before. 
*/
void editorRefreshScreen()
{
    editorScroll();

    struct abuf ab = ABUF_INIT;
    char buf[32];

    // hides cursor
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, 
            (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/* This function sets a status message for the user at the bottom of the 
    screen, where the status bar is located.
*/
void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}


/* input */
/* This function displays a prompt for the user in the status bar,
    the user can input text after the prompt such as a file name.
*/
char *editorPrompt(char *prompt, void (*callback)(char *, int))
{
    //variable declaration/assignment
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    while(1)
    {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();

        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if(buflen != 0)
                buf[--buflen] = '\0';
        }
        else if(c == '\x1b')
        {
            editorSetStatusMessage("");
            if(callback)
                callback(buf, c);
            free(buf);
            return NULL;
        }
        else if(c == '\r')
        {
            if(buflen != 0)
            {
                editorSetStatusMessage("");
                if(callback)
                    callback(buf, c);
                return buf;
            }
        }
        else if(!iscntrl(c) && c < 128)
        {
            if(buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if(callback)
            callback(buf, c);
    }
}

/* This function allows the user to move the cursor within the text editor
    using the arrow keys 
*/
void editorMoveCursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key)
    {
        case ARROW_LEFT:
            if(E.cx != 0)
            {
                E.cx--;
            }
            else if(E.cy > 0)
            {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size)
            {
                E.cx++;
            }
            else if(row && E.cx == row->size)
            {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0)
            {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows)
            {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}

/* This function handles any keys that the user might press, it handles 
    numerous ctrl key combinations and other special keys such as 'home' or
    'end' keys
*/
void editorProcessKeypress()
{
    static int quit_times = KILO_QUIT_TIMES;
    int c = editorReadKey();
    switch(c)
    {
        case '\r':
            editorInsertNewline();
            break;

        case CTRL_KEY('q'):
            if(E.dirty && quit_times > 0)
            {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                        "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if(E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case BACKSPACE: case CTRL_KEY('h'): case DEL_KEY:
            if(c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP: case PAGE_DOWN:
            {
                if(c == PAGE_UP)
                {
                    E.cy = E.rowoff;
                }
                else if(c == PAGE_DOWN)
                {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if(E.cy > E.numrows)
                        E.cy = E.numrows;
                }
                int times = E.screenrows;
                while(times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP: case ARROW_DOWN: case ARROW_LEFT: case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('l'): case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }
    quit_times = KILO_QUIT_TIMES;
}


/* init */
// This function simply initalizes all variables within the 'E' Struct
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numrows = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.syntax = NULL;

    if(getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    E.screenrows -= 2;
}

// main function - where the program starts
int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if(argc >= 2)
        editorOpen(argv[1]);

    // default status message displayed in the status bar
    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | "
            "Ctrl-F = find");

    while(1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}