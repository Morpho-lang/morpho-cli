/** @file linedit.c
 *  @author T J Atherton
 *
 *  @brief A simple UTF8 aware line editor with history, completion, multiline editing and syntax highlighting
 */

#include "linedit.h"

/** Maximum escape code size */
#define LINEDIT_CODESTRINGSIZE 24

/* **********************************************************************
 * Terminal
 * ********************************************************************** */

/* ----------------------------------------
 * Retrieve terminal type
 * ---------------------------------------- */

void linedit_enablerawmode(void);
void linedit_disablerawmode(void);

typedef enum {
    LINEDIT_NOTTTY,
    LINEDIT_UNSUPPORTED,
    LINEDIT_SUPPORTED
} linedit_terminaltype;

/** @brief   Compares two c strings independently of case
 *  @param[in] str1 - } strings to compare
 *  @param[in] str2 - }
 *  @returns 0 if the strings are identical, otherwise a positive or negative number indicating their lexographic order */
int linedit_cstrcasecmp(char *str1, char *str2) {
    if (str1 == str2) return 0;
    int result=0;
    
    for (char *p1=str1, *p2=str2; result==0; p1++, p2++) {
        result=tolower(*p1)-tolower(*p2);
        if (*p1=='\0') break;
    }
    
    return result;
}

/** Checks whether the current terminal is supported */
linedit_terminaltype linedit_checksupport(void) {
    /* Make sure both stdin and stdout are a tty */
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return LINEDIT_NOTTTY;
    }
     
    char *unsupported[]={"dumb","cons25","emacs",NULL};
    char *term = getenv("TERM");
    
    if (term == NULL) return LINEDIT_UNSUPPORTED;
    for (unsigned int i=0; unsupported[i]!=NULL; i++) {
        if (!linedit_cstrcasecmp(term, unsupported[i])) return LINEDIT_UNSUPPORTED;
    }
    
    return LINEDIT_SUPPORTED;
}

/* ----------------------------------------
 * Switch to/from raw mode
 * ---------------------------------------- */

/** Holds the original terminal state */
struct termios terminit;

bool termexitregistered=false;

/** @brief Enables 'raw' mode in the terminal
 *  @details In raw mode key presses are passed directly to us rather than
 *           being buffered. */
void linedit_enablerawmode(void) {
    struct termios termraw; /* Use to set the raw state */
    if (!termexitregistered) {
        atexit(linedit_disablerawmode);
        termexitregistered=true;
    }
    
    tcgetattr(STDIN_FILENO, &terminit); /** Get the original state*/
    
    termraw=terminit;
    /* Input: Turn off: IXON   - software flow control (ctrl-s and ctrl-q)
                        ICRNL  - translate CR into NL (ctrl-m)
                        BRKINT - parity checking
                        ISTRIP - strip bit 8 of each input byte */
    termraw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | BRKINT | ISTRIP);
    /* Output: Turn off: OPOST - output processing */
    termraw.c_oflag &= ~(OPOST);
    /* Character: CS8 Set 8 bits per byte */
    termraw.c_cflag |= (CS8);
    /* Turn off: ECHO   - causes keypresses to be printed immediately
                 ICANON - canonical mode, reads line by line
                 IEXTEN - literal (ctrl-v)
                 ISIG   - turn off signals (ctrl-c and ctrl-z) */
    termraw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Set return condition for control characters */
    termraw.c_cc[VMIN] = 1; termraw.c_cc[VTIME] = 0; /* 1 byte, no timer */
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termraw);
}

/** @brief Restore terminal state to normal */
void linedit_disablerawmode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminit);
    printf("\r"); /** Print a carriage return to ensure we're back on the left hand side */
}

/* ----------------------------------------
 * Get cursor position and width
 * ---------------------------------------- */

#define LINEDIT_CURSORPOSN_BUFFERSIZE 128
/** @brief Gets the cursor position */
bool linedit_getcursorposition(int *x, int *y) {
    char answer[LINEDIT_CURSORPOSN_BUFFERSIZE];
    int i=0, row=0, col=0;

    /* Report cursor location */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return false;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(answer)-1) {
        if (read(STDIN_FILENO,answer+i,1) != 1) break;
        if (answer[i] == 'R') break; // Response is 'R' terminated
        i++;
    }
    answer[i] = '\0'; // Terminal response is not null-terminated by default

    /* Parse response */
    if (answer[0] != 27 || answer[1] != '[') return false;
    if (sscanf(answer+2,"%d;%d",&row,&col) != 2) return false;
    
    if (y) *y = row; // Return result
    if (x) *x = col;
    return true;
}

/** @brief Gets the terminal width */
void linedit_getterminalwidth(lineditor *edit) {
    struct winsize ws;
    
    edit->ncols=80;
    
    /* Try ioctl first */
    if (!(ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)) {
        edit->ncols=ws.ws_col;
    } else {
        // Should get cursor position etc here.
    }
}

/* ----------------------------------------
 * Detect if keypresses are available
 * ---------------------------------------- */

/** Detect if a keypress is available; non-blocking */
bool linedit_keypressavailable(void) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    struct timeval timeout={ .tv_sec=0, .tv_usec=0 };
    
    return (select(1, &readfds, NULL, NULL, &timeout)>0);
}

/* ----------------------------------------
 * Output
 * ---------------------------------------- */

/** @brief Writes a string to the terminal */
bool linedit_write(char *string) {
    size_t length=strlen(string);
    if (write(STDOUT_FILENO, string, length)==-1) {
        fprintf(stderr, "Error writing to terminal.\n");
        return false;
    }
    return true;
}

/** @brief Writes a character to the terminal */
bool linedit_writechar(char c) {
    if (write(STDOUT_FILENO, &c, 1)==-1) {
        fprintf(stderr, "Error writing to terminal.\n");
        return false;
    }
    return true;
}

/** @brief Erases the current line */
bool linedit_eraseline(void) {
    return linedit_write("\033[2K");
}

/** @brief Erases the rest of the current line */
bool linedit_erasetoendofline(void) {
    return linedit_write("\033[0K");
}

/** @brief Moves the cursor to the start of the line */
bool linedit_home(void) {
    return linedit_write("\r");
}

/** @brief Sets default text */
bool linedit_defaulttext(void) {
    return linedit_write("\033[0m");
}

/** @brief Line feed */
bool linedit_linefeed(void) {
    return linedit_write("\n");
}

/** @brief Moves the cursor to the specified position */
bool linedit_movetocolumn(int posn) {
    char code[LINEDIT_CODESTRINGSIZE];
    if (posn>0) {
        sprintf(code, "\r\033[%iC", posn);
        return linedit_write(code);
    }
    return true;
}

/** @brief Moves the cursor up by n lines */
bool linedit_moveup(int n) {
    char code[LINEDIT_CODESTRINGSIZE];
    if (n>0) {
        sprintf(code, "\033[%iA", n);
        return linedit_write(code);
    }
    return true;
}

/** @brief Moves the cursor down by n lines */
 bool linedit_movedown(int n) {
    char code[LINEDIT_CODESTRINGSIZE];
    if (n>0) {
        sprintf(code, "\033[%iB", n);
        return linedit_write(code);
    }
     return true;
}

/* **********************************************************************
 * Unicode support
 * ********************************************************************** */

/* ----------------------------------------
 * Basic UTF8 support
 * ---------------------------------------- */

/** @brief Returns the number of bytes in the next character of a given utf8 string
    @returns number of bytes */
int linedit_utf8numberofbytes(char *string) {
    if (!string) return 0;
    uint8_t byte = * ((uint8_t *) string);
    
    if ((byte & 0xc0) == 0x80) return 0; // In the middle of a utf8 string
    
    // Get the number of bytes from the first character
    if ((byte & 0xf8) == 0xf0) return 4;
    if ((byte & 0xf0) == 0xe0) return 3;
    if ((byte & 0xe0) == 0xc0) return 2;
    return 1;
}

/** Decodes a utf8 encoded character pointed to by c into an int */
int linedit_utf8toint(char *c) {
    unsigned int ret = -1;
    int nbytes=linedit_utf8numberofbytes(c);
    switch (nbytes) {
        case 1: ret=(c[0] & 0x7f); break;
        case 2: ret=((c[0] & 0x1f)<<6) | (c[1] & 0x3f); break;
        case 3: ret=((c[0] & 0x0f)<<12) | ((c[1] & 0x3f)<<6) | (c[2] & 0x3f); break;
        case 4: ret=((c[0] & 0x0f)<<18) | ((c[1] & 0x3f)<<12) | ((c[2] & 0x3f)<<6) | (c[3] & 0x3f) ; break;
        default: break;
    }
    
    return ret;
}

/** @brief Utf8 character loop advancer.
    @details Determines the number of bytes for the code point at c, and advances the counter i by that number.
     Returns true if the number of bytes is >0 */
bool linedit_utf8next(char *c, int *i) {
    int adv = linedit_utf8numberofbytes(c);
    if (adv) *i+=adv;
    return adv;
}

/** @brief Count the number of UTF characters in a string
    @param[in] start - start of string
    @param[in] length - length of string
    @param[out] count - number of UTF characters
    @returns true on success */
bool linedit_utf8count(char *start, size_t length, size_t *count) {
    size_t len, n=0;
    for (char *c=start; c<start+length && *c!='\0'; c+=len, n++) {
        len=linedit_utf8numberofbytes(c);
        if (!len) return false;
    }
    *count=n;
    
    return true;
}

/* ----------------------------------------
 * Grapheme width dictionary
 * ---------------------------------------- */

void linedit_graphemeinit(linedit_graphemedictionary *dict) {
    dict->count=0;
    dict->capacity=0;
    dict->contents=NULL;
}

void linedit_graphemeclear(linedit_graphemedictionary *dict) {
    for (int i=0; i<dict->capacity; i++) {
        if (dict->contents[i].grapheme) free(dict->contents[i].grapheme);
    }
    linedit_graphemeinit(dict);
}

uint32_t linedit_hashstring(const char* key, size_t length) {
    uint32_t hash = 2166136261u; // String hashing function FNV-1a

    for (unsigned int i=0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619u; // FNV prime number for 32 bits
    }
    
    return hash;
}

bool linedit_graphemeinsert(linedit_graphemedictionary *dict, char *grapheme, size_t length, int width);

bool linedit_graphemeresize(linedit_graphemedictionary *dict, int size) {
    linedit_graphemeentry *new=malloc(size*sizeof(linedit_graphemeentry)), *old=dict->contents;
    int osize=dict->capacity;

    if (new) { // Clear the newly allocated structure
        for (unsigned int i=0; i<size; i++) {
            new[i].grapheme = NULL;
            new[i].width = 0;
        }
    } else return false;
    
    dict->capacity=size;
    dict->contents=new;
    dict->count=0;
    
    if (old) { // Copy old contents across
        for (unsigned int i=0; i<osize; i++) {
            if (old[i].grapheme) if (!linedit_graphemeinsert(dict, old[i].grapheme, strlen(old[i].grapheme), old[i].width)) return false;
        }
    }
    return true;
}

bool linedit_graphemefind(linedit_graphemedictionary *dict, char *grapheme, size_t length, int *posn) {
    if (!dict->contents) return false;
    uint32_t hash = linedit_hashstring(grapheme, length);
    
    int start = hash % dict->capacity;
    int i=start;
    
    do {
        if (!dict->contents[i].grapheme) { // Blank entry -> not found
            if (posn) *posn = i;
            return false;
        }
        
        if (strncmp(grapheme, dict->contents[i].grapheme, length)==0) { // Found
            if (posn) *posn = i;
            return true;
        }
        
        i = (i+1) % dict->capacity;
    } while (i!=start); // Loop terminates once we return to the starting position
    
    return false;
}

#define LINEDIT_MINDICTIONARYSIZE 8
#define LINEDIT_SIZEINCREASETHRESHOLD(x) (((x)>>1) + ((x)>>2))
#define LINEDIT_INCREASEDICTIONARYSIZE(x) (2*x)

bool linedit_graphemeinsert(linedit_graphemedictionary *dict, char *grapheme, size_t length, int width) {
    
    if (!dict->contents) {
        if (!linedit_graphemeresize(dict, LINEDIT_MINDICTIONARYSIZE)) return false;
    } else if (dict->count+1 > LINEDIT_SIZEINCREASETHRESHOLD(dict->capacity)) {
        if (!linedit_graphemeresize(dict, LINEDIT_INCREASEDICTIONARYSIZE(dict->capacity))) return false;
    }
    
    int posn;
    if (linedit_graphemefind(dict, grapheme, length, &posn)) return true;
    
    char *label = strndup(grapheme, length);
    if (!label) return false;
    dict->contents[posn].grapheme=label;
    dict->contents[posn].width=width;
    dict->count++;
    return true;
}

bool linedit_graphemelookup(linedit_graphemedictionary *dict, char *grapheme, size_t length, int *width) {
    if (!dict->contents) return false;
    int posn;
    if (!linedit_graphemefind(dict, grapheme, length, &posn)) return false;
    if (width) *width = dict->contents[posn].width;
    return true;
}

void linedit_graphemeshow(linedit_graphemedictionary *dict) {
    for (int i=0; i<dict->capacity; i++) {
        if (dict->contents[i].grapheme) {
            printf("%s: %i\n", dict->contents[i].grapheme, dict->contents[i].width);
        }
    }
}

/* ----------------------------------------
 * Grapheme display width
 * ---------------------------------------- */

/** @brief Identifies the current grapheme length */
size_t linedit_graphemelength(lineditor *edit, char *str) {
    if (*str=='\0') return 0; // Ensure we return on null terminator
    if (edit->graphemefn) return edit->graphemefn(str, SIZE_MAX);
    return (size_t) linedit_utf8numberofbytes(str); // Fallback on displaying unicode chars one by one
}

/** @brief Returns the display with of a grapheme sequence if known */
bool linedit_graphemedisplaywidth(lineditor *edit, char *grapheme, size_t length, int *width) {
    if (length==1) {
        if (iscntrl(*grapheme)) return 0;
        return 1;
    }
    
    return linedit_graphemelookup(&edit->graphemedict, grapheme, length, width);
}

/** @brief Renders a grapheme sequence, measuring its display width */
bool linedit_graphememeasurewidth(lineditor *edit, char *grapheme, size_t length, int *width) {
    int x0=0, x1=0, w=0;
    linedit_getcursorposition(&x0, NULL);
    write(STDOUT_FILENO, grapheme, length);
    linedit_getcursorposition(&x1, NULL);
    w=(x1>x0 ? x1-x0 : 1);
    *width = w;
    return linedit_graphemeinsert(&edit->graphemedict, grapheme, length, w);
}

/* **********************************************************************
 * Rendering
 * ********************************************************************** */

/** @brief Renders a string, showing only characters in columns l...r */
void linedit_renderstring(lineditor *edit, char *string, int l, int r) {
    int i=0;
    size_t length=0;
    
    for (char *s=string; *s!='\0'; s+=length) {
        length = linedit_graphemelength(edit, s);
        
        if (length==0) break;
        if (*s=='\r') { // Reset on a carriage return
            if (write(STDOUT_FILENO, "\r" , 1)==-1) return;
            i=0;
        } else if (*s=='\n') { // Clear to end of line and return
            if (!linedit_write("\x1b[K\n\r")) return;
            if (write(STDOUT_FILENO, edit->cprompt.string, edit->cprompt.length)==-1) return;
            i=0;
        } else if (*s=='\t') {
            if (!linedit_write(" ")) return;
            i+=1;
        } else if (iscntrl(*s)) {
            if (*s=='\033') { // A terminal control character
                char *ctl=s; // First identify its length
                while (!isalpha(*ctl) && *ctl!='\0') ctl++;
                if (write(STDOUT_FILENO, s, ctl-s+1)==-1) return; // print it
                s=ctl;
            }
        } else { // Otherwise show printable characters that lie within the window
            int width=1;
            if (!linedit_graphemedisplaywidth(edit, s, length, &width)) {
                linedit_graphememeasurewidth(edit, s, length, &width);
            } else if (i>=l && i<r) {
                if (write(STDOUT_FILENO, s, length)==-1) return;
            }
            i+=1;
        }
    }
}

/* **********************************************************************
 * Strings
 * ********************************************************************** */

#define linedit_MINIMUMSTRINGSIZE  8

/** Initializes a string, clearing all fields */
void linedit_stringinit(linedit_string *string) {
    string->capacity=0;
    string->length=0;
    string->next=NULL;
    string->string=NULL;
}

/** Clears a string, deallocating memory if necessary */
void linedit_stringclear(linedit_string *string) {
    if (string->string) free(string->string);
    linedit_stringinit(string);
}

/** @brief Finds the index of character i in a utf8 encoded string.
 * @param[in] string - string to index
 * @param[in] i - Index of character to find
 * @param[in] offset - Offset into the string - set to 0 to count from the start of the string
 * @param[out] out - offset in bytes from offset to character i */
bool linedit_stringutf8index(linedit_string *string, size_t i, size_t offset, size_t *out) {
    int advance=0;
    size_t nchars=0;
    
    for (size_t j=0; j+offset<=string->length; j+=advance, nchars++) {
        if (nchars==i) { *out = j; return true; }
        advance=linedit_utf8numberofbytes(string->string+offset+j);
        if (advance==0) break; // If advance is 0, the string is corrupted; return failure
    }
    
    return false;
}

/** Resizes a string
 *  @param string   - the string to grow
 *  @param size     - requested size
 *  @returns true on success, false on failure */
bool linedit_stringresize(linedit_string *string, size_t size) {
    size_t newsize=linedit_MINIMUMSTRINGSIZE;
    char *old=string->string;
    
    /* If we're increasing the size, grow by factors of 1.5 to avoid excessive calls to allocator */
    while (newsize<=size) newsize=((newsize<<1)+newsize)>>1; // mul by x1.5
        
    string->string=realloc(string->string,newsize);
    if (string->string) {
        if (!old) {
            string->string[0]='\0'; /* Make sure a new string is zero-terminated */
            string->length=0;
        }
        string->capacity=newsize;
    }
    return (string->string!=NULL);
}

/** Adds a string to a string */
void linedit_stringappend(linedit_string *string, char *c, size_t nbytes) {
    if (string->capacity<=string->length+nbytes) {
        if (!linedit_stringresize(string, string->length+nbytes+1)) return;
    }
    
    strncpy(string->string+string->length, c, nbytes);
    string->length+=nbytes;
    string->string[string->length]='\0'; /* Keep the string zero-terminated */
}

/** @brief   Inserts characters at a given position
 *  @param[in] string - string to amend
 *  @param[in] posn - insertion position as a character index
 *  @param[in] c - string to insert
 *  @param[in] n - number of bytes to insert
 *  @details If the position is after the length of the string
 *           the new characters are instead appended. */
void linedit_stringinsert(linedit_string *string, size_t posn, char *c, size_t n) {
    size_t offset;
    if (!linedit_stringutf8index(string, posn, 0, &offset)) return;
    
    if (offset<string->length) {
        if (string->capacity<=string->length+n) {
            if (!linedit_stringresize(string, string->length+n+1)) return;
        }
        /* Move the remaining part of the string */
        memmove(string->string+offset+n, string->string+offset, string->length-posn+1);
        /* Copy in the text to insert */
        memmove(string->string+offset, c, n);
        string->length+=n;
    } else {
        linedit_stringappend(string, c, n);
    }
}

/** @brief   Deletes characters at a given position.
 *  @param[in] string - string to amend
 *  @param[in] posn - Delete characters as a character index
 *  @param[in] n - number of characters to delete */
void linedit_stringdelete(linedit_string *string, size_t posn, size_t n) {
    size_t offset;
    size_t nbytes;
    
    if (string->length<n) return;
    
    if (!linedit_stringutf8index(string, posn, 0, &offset)) return;
    if (!linedit_stringutf8index(string, n, offset, &nbytes)) return;
    
    if (offset<string->length) {
        if (offset+nbytes<string->length) {
            memmove(string->string+offset, string->string+offset+nbytes, string->length-offset-nbytes+1);
        } else {
            string->string[offset]='\0';
        }
        string->length=strlen(string->string);
    }
}

/** Adds a c string to a string */
void linedit_stringaddcstring(linedit_string *string, char *s) {
    if (!s || !string) return;
    size_t size=strlen(s);
    if (string->capacity<=string->length+size+1) {
        if (!linedit_stringresize(string, string->length+size+1)) return;
    }
    
    strncpy(string->string+string->length, s, string->capacity-string->length);
    string->length+=size;
}

/** Finds the length of a string in unicode characters */
int linedit_stringlength(linedit_string *string) {
    size_t count=0;
    linedit_utf8count(string->string, string->length, &count);
    return (int) count;
}

/** Finds the display width of a string */
int linedit_stringdisplaywidth(lineditor *edit, linedit_string *string) {
    int width=0;
    size_t len;
    for (int i=0; i<string->length; i+=len) {
        int w=1;
        len = linedit_graphemelength(edit, string->string+i);
        linedit_graphemedisplaywidth(edit, string->string+i, len, &w);
        width+=w;
    }
    return width;
}

/** Finds the display coordinates for a given position in a string */
void linedit_stringdisplaycoordinates(lineditor *edit, linedit_string *string, int posn, int *xout, int *yout) {
    int x=0, y=0, n=0;
    size_t count;
    for (int i=0; i<string->length; n+=count) {
        if (n>=posn) break;
        
        char *c=string->string+i;
        size_t len = linedit_graphemelength(edit, c);
        if (!linedit_utf8count(c, len, &count)) break;
        
        if (*c=='\n') {
            x=0; y++;
        } else {
            int w=1;
            linedit_graphemedisplaywidth(edit, string->string+i, len, &w);
            x+=w;
        }

        i+=len;
        if (!len) break;
    }
    if (xout) *xout = x;
    if (yout) *yout = y;
}

/** Finds the line and unicode character number for a given position in the string.
 * @param[in] string - the string
 * @param[in] posn - character position in the string
 * @param[out] xout - x coordinates corresponding to posn n
 * @param[out] yout - y */
void linedit_stringcoordinates(linedit_string *string, int posn, int *xout, int *yout) {
    int x=0, y=0, n=0;
    for (int i=0; i<string->length; n++) {
        if (n==posn) break;
        char *c=string->string+i;

        if (*c=='\n') {
            x=0; y++;
        } else x++;

        if (!linedit_utf8next(c, &i)) break;
    }
    if (xout) *xout = x;
    if (yout) *yout = y;
}

/** Finds the position in the string for specified coordinates
 * @param[in] string - the string
 * @param[in] x - character position or -1 to find the end of the line
 * @param[in] y - line number
 * @param[out] posn - position corresponding to   */
void linedit_stringfindposition(linedit_string *string, int x, int y, int *posn) {
    int xx=0, yy=0, n=0;
    for (int i=0; i<string->length; n++) {
        if (xx==x && yy==y) break;
        
        char *c = string->string+i;
        if (*c=='\n') {
            xx=0; yy++;
            if (yy>y) break;
        } else xx++;
        
        if (!linedit_utf8next(c, &i)) break;
    }
    if (posn) *posn = n;
}
/** Locates a particular posn in a string, returning a pointer to the character */
char *linedit_stringlocate(linedit_string *string, int posn) {
    size_t out;
    if (!linedit_stringutf8index(string, posn, 0, &out)) return NULL;
    return string->string+out;
}

/** Counts the number of lines a string occupies */
int linedit_stringcountlines(linedit_string *string) {
    int lines;
    linedit_stringcoordinates(string, -1, NULL, &lines);
    return lines;
}

/** Returns a C string from a string */
char *linedit_cstring(linedit_string *string) {
    return string->string;
}

/** Creates a new string from a C string */
linedit_string *linedit_newstring(char *string) {
    linedit_string *new = malloc(sizeof(linedit_string));
    
    if (new) {
        linedit_stringinit(new);
        linedit_stringaddcstring(new, string);
    }
    
    return new;
}

/* **********************************************************************
 * Lists of strings
 * ********************************************************************** */

/** Adds an entry to a string list */
void linedit_stringlistadd(linedit_stringlist *list, char *string) {
    linedit_string *new=linedit_newstring(string);
    
    if (new) {
        new->next=list->first;
        list->first=new;
    }
}

/** Initializes a string list */
void linedit_stringlistinit(linedit_stringlist *list) {
    list->first=NULL;
    list->posn=0;
}

/** Frees the contents of a string list */
void linedit_stringlistclear(linedit_stringlist *list) {
    while (list->first!=NULL) {
        linedit_string *s = list->first;
        list->first=s->next;
        linedit_stringclear(s);
        free(s);
    }
    linedit_stringlistinit(list);
}

/** Removes a string from a list */
void linedit_stringlistremove(linedit_stringlist *list, linedit_string *string) {
    linedit_string *s=NULL, *prev=NULL;
    
    for (s=list->first; s!=NULL; s=s->next) {
        if (s==string) {
            if (prev) {
                prev->next=s->next;
            } else {
                list->first=s->next;
            }
            linedit_stringclear(s);
            free(s);
            return;
        }
        
        prev=s;
    }
}

/** Chooses an element of a stringlist
 * @param[in]  list the list to select from
 * @param[in]  n    entry number to select
 * @parma[out] *m   entry number actually selected
 * @returns the selected element */
linedit_string *linedit_stringlistselect(linedit_stringlist *list, unsigned int n, unsigned int *m) {
    unsigned int i=0;
    linedit_string *s=NULL;
    
    for (s=list->first; s!=NULL && s->next!=NULL; s=s->next) {
        if (i==n) break;
        i++;
    }
    
    if (m) *m=i;
    
    return s;
}

/** Count the number of entries in a string list */
int linedit_stringlistcount(linedit_stringlist *list) {
    int n=0;
    for (linedit_string *s=list->first; s!=NULL; s=s->next) n++;
    return n;
}

/* **********************************************************************
 * History list
 * ********************************************************************** */

/** Adds an entry to the history list */
void linedit_historyadd(lineditor *edit, char *string) {
    linedit_stringlistadd(&edit->history, string);
}

/** Frees the history list */
void linedit_historyclear(lineditor *edit) {
    linedit_stringlistclear(&edit->history);
}

/** Makes a particular history entry current */
unsigned int linedit_historyselect(lineditor *edit, unsigned int n) {
    unsigned int m=n;
    linedit_string *s=linedit_stringlistselect(&edit->history, n, &m);
    
    if (s) {
        edit->current.length=0;
        linedit_stringaddcstring(&edit->current, s->string);
    }
    
    return m;
}

/** Advances the history list */
void linedit_historyadvance(lineditor *edit, unsigned int n) {
    edit->history.posn+=n;
    edit->history.posn=linedit_historyselect(edit, edit->history.posn);
}

/** Returns the number of entries in the history list */
int linedit_historycount(lineditor *edit) {
    return linedit_stringlistcount(&edit->history);
}

/* **********************************************************************
 * Autocompletion
 * ********************************************************************** */

bool linedit_atend(lineditor *edit);

/** Regenerates the list of autocomplete suggestions */
void linedit_generatesuggestions(lineditor *edit) {
    if (edit->completer) {
        linedit_stringlistclear(&edit->suggestions);
        
        if (edit->current.string &&
            linedit_atend(edit)) {
            (edit->completer) (edit->current.string, edit->cref, &edit->suggestions);
        }
    }
}

/** Check whether any suggestions are available */
bool linedit_aresuggestionsavailable(lineditor *edit) {
    return (edit->suggestions.first!=NULL);
}

/** Get the current suggestion */
char *linedit_currentsuggestion(lineditor *edit) {
    linedit_string *s=linedit_stringlistselect(&edit->suggestions, edit->suggestions.posn, NULL);
    
    if (s) return s->string;
    
    return NULL;
}

/** Advance through the suggestions */
void linedit_advancesuggestions(lineditor *edit, unsigned int n) {
    unsigned int rposn=edit->suggestions.posn+n, nposn=rposn;
    linedit_stringlistselect(&edit->suggestions, rposn, &nposn);
    edit->suggestions.posn=nposn;
    if (rposn!=edit->suggestions.posn) edit->suggestions.posn=0; /* Go back to first */
}

/* **********************************************************************
 * Multiline mode
 * ********************************************************************** */

/** Test whether we should enter multiline editing mode */
bool linedit_shouldmultiline(lineditor *edit) {
    if (edit->multiline && edit->current.string) return (edit->multiline) (edit->current.string, edit->mlref);
    return false; 
}

/* **********************************************************************
 * Syntax highlighting
 * ********************************************************************** */

/** @brief Writes a control sequence to reset default text */
void linedit_stringdefaulttext(linedit_string *out) {
    char code[LINEDIT_CODESTRINGSIZE];
    sprintf(code, "\033[0m");
    linedit_stringaddcstring(out, code);
}

/** @brief Writes a control sequence to set a given color */
void linedit_stringsetcolor(linedit_string *out, linedit_color col) {
    char code[LINEDIT_CODESTRINGSIZE];
    sprintf(code, "\033[%im", (col==LINEDIT_DEFAULTCOLOR ? 0: 30+col));
    linedit_stringaddcstring(out, code);
}

/** @brief Writes a control sequence to set a given emphasis */
void linedit_stringsetemphasis(linedit_string *out, linedit_emphasis emph) {
    char code[LINEDIT_CODESTRINGSIZE];
    switch (emph) {
        case LINEDIT_BOLD: sprintf(code, "\033[1m"); break;
        case LINEDIT_UNDERLINE: sprintf(code, "\033[4m"); break;
        case LINEDIT_REVERSE: sprintf(code, "\033[7m"); break;
        case LINEDIT_NONE: break;
    }
    
    linedit_stringaddcstring(out, code);
}

/** Adds a string with selection highlighting
 * @param[in] edit - active editor
 * @param[in] in - input string
 * @param[in] offset - offset of string in characters
 * @param[in] length - length of string in characters
 * @param[in] col - color
 * @param[out] out - display plus coloring information written to this string */
void linedit_addcstringwithselection(lineditor *edit, char *in, size_t offset, size_t length, linedit_color *col, linedit_string *out) {
    int lposn=-1, rposn=-1;
    
    /* If a selection is active, discover its bounds */
    if (edit->mode==LINEDIT_SELECTIONMODE && !(edit->sposn<0)) {
        lposn=(edit->sposn < edit->posn ? edit->sposn : edit->posn) - (int) offset;
        rposn = (edit->sposn < edit->posn ? edit->posn : edit->sposn) - (int) offset;
    }
    
    /* Set the color if provided */
    if (col) linedit_stringsetcolor(out, *col);
    
    /* Is the text we're showing outside the selected region entirely? */
    if (rposn<0 || lposn>(int) length) {
        linedit_stringappend(out, in, length);
    } else {
        /* If not, add the characters one by one and insert highlighting */
        if (lposn<0) {
            linedit_stringsetemphasis(out, LINEDIT_REVERSE);
        }
        char *c=in;
        for (int i=0; i<length; i++) {
            if (i==lposn) {
                linedit_stringsetemphasis(out, LINEDIT_REVERSE);
            }
            if (i==rposn) {
                linedit_stringdefaulttext(out);
                /* Restore the color if one is provided */
                if (col) linedit_stringsetcolor(out, *col);
            }
            int nbytes=linedit_utf8numberofbytes(c);
            if (nbytes) linedit_stringappend(out, c, nbytes);
            else nbytes=1; // Corrupted stream
            c+=nbytes;
        }
    }
}

/** Compare two colormap structs */
int linedit_colormapcmp(const void *l, const void *r) {
    linedit_colormap *a = (linedit_colormap *) l;
    linedit_colormap *b = (linedit_colormap *) r;
    
    return a->type - b->type;
}

/** Returns a color matching tokentype type */
linedit_color linedit_colorfromtokentype(lineditor *edit, linedit_tokentype type) {
    linedit_colormap key = { .type = type, .col = LINEDIT_DEFAULTCOLOR };
    
    if (edit->color) {
        linedit_colormap *val = bsearch(&key, edit->color->col, edit->color->ncols, sizeof(linedit_colormap), linedit_colormapcmp);
        
        if (val) return val->col;
    }
    
    return LINEDIT_DEFAULTCOLOR;
}

/** Print a string with syntax coloring */
void linedit_syntaxcolorstring(lineditor *edit, linedit_string *in, linedit_string *out) {
    linedit_tokenizefn tokenizer=edit->color->tokenizer;
    linedit_color col=LINEDIT_DEFAULTCOLOR;
    linedit_token tok;
    unsigned int iter=0;
    int line=0;
    
    for (char *c=in->string; c!=NULL && *c!='\0';) {
        bool success = (tokenizer) (c, edit->color->tokref, &tok);
        /* Get the next token */
        if (success && tok.length>0 && tok.start>=c) {
            size_t padding=tok.start-c;
            /* If there's leading unrecognized characters, print them. */
            if (tok.start>c) {
                col=LINEDIT_DEFAULTCOLOR;
                linedit_addcstringwithselection(edit, c, c-in->string, padding, &col, out);
            }
            
            /* Set the color */
            col = linedit_colorfromtokentype(edit, tok.type);
            
            /* Copy the token across */
            if (tok.length>0) {
                linedit_addcstringwithselection(edit, tok.start, tok.start-in->string, tok.length, &col, out);
            }
            
            c=tok.start+tok.length;
        } else {
            col=LINEDIT_DEFAULTCOLOR;
            linedit_addcstringwithselection(edit, c, c-in->string, in->length-(c-in->string), &col, out);
            return;
        };
        iter++;
        if (iter>in->length) {
            if (!edit->color->lexwarning) {
                fprintf(stderr, "\n\rLinedit error: Syntax colorer appears to be stuck in an infinite loop; ensure the tokenizer returns false if it doesn't recognize a token.\n");
                edit->color->lexwarning=true;
            }
            return;
        }
    }
}

/** Print a string without syntax coloring */
void linedit_plainstring(lineditor *edit, linedit_string *in, linedit_string *out) {
    linedit_addcstringwithselection(edit, in->string, 0, in->length, NULL, out);
}

/* **********************************************************************
 * Keypresses
 * ********************************************************************** */

/** Identifies the type of keypress */
typedef enum {
    UNKNOWN, CHARACTER,
    RETURN, TAB, DELETE,
    UP, DOWN, LEFT, RIGHT,   // Arrow keys
    HOME, END,               // Home and End
    SHIFT_LEFT, SHIFT_RIGHT, // Shift+arrow key
    CTRL,
} keytype;

/** A single keypress event obtained and processed by the terminal */
typedef struct {
    keytype type; /** Type of keypress */
    char c[5]; /** Up to four bytes of utf8 encoded unicode plus null terminator */
    int nbytes; /** Number of bytes */
} keypress;

#define LINEDIT_KEYPRESSGETCHAR(a) ((a)->c[0])

/** Raw codes produced by the terminal */
enum keycodes {
    TAB_CODE = 9,      // Tab
    RETURN_CODE = 13,  // Enter or return
    ESC_CODE = 27,     // Escape
    DELETE_CODE = 127  // Delete
};

/** Enable this macro to get reports on unhandled keypresses */
//#define LINEDIT_DEBUGKEYPRESS

/** Initializes a keypress structure */
void linedit_keypressinit(keypress *out) {
    out->type=UNKNOWN;
    for (int i=0; i<5; i++) out->c[i]='\0';
    out->nbytes=0;
}

/** @brief Read and decode a single keypress from the terminal */
bool linedit_readkey(lineditor *edit, keypress *out) {
    out->type=UNKNOWN;
    
    if (read(STDIN_FILENO, out->c, 1) == 1) {
        if (iscntrl(LINEDIT_KEYPRESSGETCHAR(out))) {
            switch (LINEDIT_KEYPRESSGETCHAR(out)) {
                case ESC_CODE:
                {   /* Escape sequences */
                    char seq[LINEDIT_CODESTRINGSIZE];
                    ssize_t ret=0;
                    
                    /* Read in the escape sequence */
                    for (unsigned int i=0; i<LINEDIT_CODESTRINGSIZE; i++) {
                        ret=read(STDIN_FILENO, &seq[i], 1);
                        if (ret<0 || isalpha(seq[i])) break;
                    }
                    
                    /** Decode the escape sequence */
                    if (seq[0]=='[') {
                        if (isdigit(seq[1])) { /* Extended seqence */
                            if (strncmp(seq, "[1;2C", 5)==0) {
                                out->type=SHIFT_RIGHT;
                            } else if (strncmp(seq, "[1;2D", 5)==0) {
                                out->type=SHIFT_LEFT;
                            } else {
#ifdef LINEDIT_DEBUGKEYPRESS
                                printf("Extended escape sequence: ");
                                for (unsigned int i=0; i<10; i++) {
                                    printf("%c", seq[i]);
                                    if (isalpha(seq[i])) break;
                                }
                                printf("\n");
#endif
                            }
                        } else {
                            switch (seq[1]) {
                                case 'A': out->type=UP; break;
                                case 'B': out->type=DOWN; break;
                                case 'C': out->type=RIGHT; break;
                                case 'D': out->type=LEFT; break;
                                default:
#ifdef LINEDIT_DEBUGKEYPRESS
                                    printf("Unhandled escape sequence: %c%c%c\r\n", seq[0], seq[1], seq[2]);
#endif
                                    break;
                            }
                        }
                    }
                }
                    break;
                case TAB_CODE:    out->type=TAB; break;
                case DELETE_CODE: out->type=DELETE; break;
                case RETURN_CODE: out->type=RETURN; break;
                default:
                    if (LINEDIT_KEYPRESSGETCHAR(out)>0 && LINEDIT_KEYPRESSGETCHAR(out)<27) { /* Ctrl+character */
                        out->type=CTRL;
                        out->c[0]+='A'-1; /* Return the character code */
#ifdef LINEDIT_DEBUGKEYPRESS
                        printf("Ctrl+character: %c\r\n", out->c[0]);
#endif
                    } else {
#ifdef LINEDIT_DEBUGKEYPRESS
                        printf("Unhandled keypress: %d\r\n", LINEDIT_KEYPRESSGETCHAR(out));
#endif
                    }
            }
            
        } else {
            out->nbytes=linedit_utf8numberofbytes(out->c);
            /* Read in the unicode sequence */
            ssize_t ret=0;
            for (int i=1; i<out->nbytes; i++) {
                ret=read(STDIN_FILENO, &out->c[i], 1);
                if (ret<0) break;
            }
            out->type=CHARACTER;
#ifdef LINEDIT_DEBUGKEYPRESS
            printf("Character: %s (%i bytes)\r\n", out->c, out->nbytes);
#endif
        }
    }
    return true;
}

/* **********************************************************************
 * The line editor
 * ********************************************************************** */

/* ----------------------------------------
 * Get and set editor state
 * ---------------------------------------- */

/** @brief Gets the current mode */
lineditormode linedit_getmode(lineditor *edit) {
    return edit->mode;
}

/** @brief Sets the current mode, setting/clearing any state dependent data  */
void linedit_setmode(lineditor *edit, lineditormode mode) {
    if (mode!=LINEDIT_HISTORYMODE) {
        if (edit->mode==LINEDIT_HISTORYMODE) {
            linedit_stringlistremove(&edit->history, edit->history.first);
        }
        edit->history.posn=0;
    }
    if (mode==LINEDIT_SELECTIONMODE) {
        if (edit->sposn<0) edit->sposn=edit->posn;
    } else {
        edit->sposn=-1;
    }
    edit->mode=mode;
}

/** Sets the current position
 * @param edit     - the editor
 * @param posn     - position to set, or negative to move to end */
void linedit_setposition(lineditor *edit, int posn) {
    edit->posn=(posn<0 ? linedit_stringlength(&edit->current) : posn);
}

/** @brief Advances the position by delta
 *  @details We ensure that the current position also lies within the string. */
void linedit_advanceposition(lineditor *edit, int delta) {
    edit->posn+=delta;
    if (edit->posn<0) edit->posn=0;
    int linewidth = linedit_stringlength(&edit->current);
    if (edit->posn>linewidth) edit->posn=linewidth;
}

/** @brief Checks if we're at the end of the input */
bool linedit_atend(lineditor *edit) {
    return (edit->posn==linedit_stringlength(&edit->current));
}

/** @brief Checks if we're at a newline character */
bool linedit_atnewline(lineditor *edit) {
    char *c=linedit_stringlocate(&edit->current, edit->posn);
    return (c && *c=='\n');
}

/* ----------------------------------------
 * Redraw
 * ---------------------------------------- */

/** Refreshes the display */
void linedit_redraw(lineditor *edit) {
    int sugglength=0;
    linedit_string output; /* Holds the output string */
    linedit_stringinit(&output);
    
    // Render the current buffer to the output string
    linedit_stringdefaulttext(&output);
    
    if (edit->color) {
        linedit_syntaxcolorstring(edit, &edit->current, &output);
    } else {
        linedit_plainstring(edit, &edit->current, &output);
    }
    
    // Display any autocompletion suggestions at the end
    if (linedit_aresuggestionsavailable(edit)) {
        char *suggestion = linedit_currentsuggestion(edit);
        linedit_stringsetemphasis(&output, LINEDIT_BOLD);
        linedit_stringaddcstring(&output, suggestion);
        sugglength=(int) strlen(suggestion);
    }
    
    // Reser default text
    linedit_stringdefaulttext(&output);
    
    // Retrieve the current editing position
    int xpos, ypos, nlines;
    linedit_stringdisplaycoordinates(edit, &edit->current, edit->posn, &xpos, &ypos);
    linedit_stringcoordinates(&edit->current, -1, NULL, &nlines);
    
    /* Determine the left and right hand boundaries */
    int promptwidth=linedit_stringdisplaywidth(edit, &edit->prompt);
    int stringwidth=linedit_stringlength(&edit->current);
    
    int start=0, end=promptwidth+stringwidth+sugglength;
    /*if (end>=edit->ncols) {
        // Are we near the start?
        if (promptwidth+edit->posn<edit->ncols) {
            start = 0;
        } else {
            start = promptwidth+edit->posn-edit->ncols+1;
        }
        end=start+edit->ncols-1;
    }*/
    
    linedit_moveup(ypos); // Move to the starting line
    linedit_home();
    linedit_defaulttext();
    linedit_write(edit->prompt.string);
    
    // Now render the output string
    linedit_renderstring(edit, output.string, start, end);
    
    linedit_erasetoendofline();
    
    linedit_moveup(nlines-ypos);  // Move to the cursor position
    linedit_movetocolumn(promptwidth+xpos-start);

    linedit_stringclear(&output);
}

/** @brief Changes the height of the current line editing session, erasing garbage if necessary */
void linedit_changeheight(lineditor *edit, int oldheight, int newheight, int oldvpos, int newvpos) {
    if (oldheight==newheight) {
        if (oldvpos<newvpos) linedit_movedown(newvpos-oldvpos);
        else linedit_moveup(oldvpos-newvpos);
    } else if (newheight>oldheight) {
        for (int i=0; i<newheight-oldheight; i++) linedit_linefeed();
    } else {
        for (int i=0; i<oldheight-oldvpos; i++) { // Erase to end
            linedit_eraseline();
            linedit_linefeed();
        }
        for (int i=0; i<oldheight-newvpos; i++) { 
            linedit_eraseline();
            linedit_moveup(1);
        }
    }
}

/** @brief Moves to the end of the buffer */
void linedit_movetoend(lineditor *edit) {
    int vpos, nlines=linedit_stringcountlines(&edit->current);
    linedit_stringcoordinates(&edit->current, edit->posn, NULL, &vpos);
    for (int i=vpos; i<nlines; i++) linedit_linefeed();
    linedit_setposition(edit, -1);
}

/* ----------------------------------------
 * Process key presses
 * ---------------------------------------- */

/** @brief Moves the current posn to the next grapheme */
void linedit_nextgrapheme(lineditor *edit) {
    char *str=linedit_stringlocate(&edit->current, edit->posn);
    size_t length=linedit_graphemelength(edit, str), count;
    
    if (!linedit_utf8count(str, length, &count)) return;
    edit->posn+=count;
}

/** @brief Moves the current posn to the previous grapheme */
void linedit_prevgrapheme(lineditor *edit) {
    char *str=linedit_stringlocate(&edit->current, edit->posn);
    char *prev=edit->current.string;
    
    size_t len=0, count;
    for (char *c=prev; c<str; c+=len) {
        len=linedit_graphemelength(edit, c);
        if (!len) return;
        prev=c;
    }
    
    if (!linedit_utf8count(prev, str-prev, &count)) return;
    edit->posn-=count;
}

/** @brief Process a left keypress */
void linedit_processarrowkeypress(lineditor *edit, lineditormode mode, int delta) {
    linedit_setmode(edit, mode);
    if (delta>0) {
        linedit_nextgrapheme(edit);
    } else linedit_prevgrapheme(edit);
    
    linedit_atnewline(edit);
}

/** @brief Change the line */
void linedit_processchangeline(lineditor *edit, int delta) {
    int x, yinit, y;
    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
    linedit_stringcoordinates(&edit->current, edit->posn, &x, &yinit);
    y=yinit+delta;
    if (y<0) y=0;
    linedit_stringfindposition(&edit->current, x, y, &edit->posn);
}

/** @brief Obtain and process a single keypress */
bool linedit_processkeypress(lineditor *edit) {
    keypress key;
    bool regeneratesuggestions=true;
    
    linedit_keypressinit(&key);
    
    do {
        if (linedit_readkey(edit, &key)) {
            switch (key.type) {
                case CHARACTER:
                    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                    linedit_stringinsert(&edit->current, edit->posn, key.c, key.nbytes);
                    linedit_advanceposition(edit, 1);
                    break;
                case DELETE:
                    if (linedit_getmode(edit)==LINEDIT_SELECTIONMODE) {
                        /* Delete the selection */
                        int lposn=(edit->sposn < edit->posn ? edit->sposn : edit->posn);
                        int rposn = (edit->sposn < edit->posn ? edit->posn : edit->sposn);
                        linedit_stringdelete(&edit->current, lposn, rposn-lposn);
                        edit->posn=lposn;
                    } else {
                        /* Delete a character */
                        if (edit->posn>0) {
                            linedit_stringdelete(&edit->current, edit->posn-1, 1);
                            linedit_advanceposition(edit, -1);
                        }
                    }
                    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                    break;
                case LEFT:
                    linedit_processarrowkeypress(edit, LINEDIT_DEFAULTMODE, -1);
                    break;
                case RIGHT:
                    linedit_processarrowkeypress(edit, LINEDIT_DEFAULTMODE, +1);
                    break;
                case SHIFT_LEFT:
                    linedit_processarrowkeypress(edit, LINEDIT_SELECTIONMODE, -1);
                    break;
                case SHIFT_RIGHT:
                    linedit_processarrowkeypress(edit, LINEDIT_SELECTIONMODE, +1);
                    break;
                case UP:
                {
                    if (linedit_getmode(edit)!=LINEDIT_HISTORYMODE) {
                        linedit_setmode(edit, LINEDIT_HISTORYMODE);
                        linedit_historyadd(edit, (edit->current.string ? edit->current.string : ""));
                    }
                    
                    linedit_historyadvance(edit, 1);
                    linedit_setposition(edit, -1);
                }
                    break;
                case DOWN:
                    if (linedit_getmode(edit)==LINEDIT_HISTORYMODE) {
                        linedit_historyadvance(edit, -1);
                        linedit_setposition(edit, -1);
                    } else if (linedit_aresuggestionsavailable(edit)) {
                        linedit_advancesuggestions(edit, 1);
                        regeneratesuggestions=false;
                    }
                    break;
                case RETURN:
                    if (linedit_shouldmultiline(edit)) {
                        linedit_stringaddcstring(&edit->current, "\n");
                        linedit_advanceposition(edit, +1);
                    } else return false;
                    break;
                case TAB:
                    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                    /* If suggestions are available (i.e. we're at the end of the line)... */
                    if (linedit_aresuggestionsavailable(edit)) {
                        char *sugg = linedit_currentsuggestion(edit);
                        if (sugg) {
                            linedit_stringaddcstring(&edit->current, sugg);
                            linedit_movetoend(edit);
                        }
                    } else { // Otherwise simply add a tab character
                        linedit_stringinsert(&edit->current, edit->posn, "\t", 1);
                        linedit_advanceposition(edit, +1);
                    }
                    break;
                case CTRL: /* Handle ctrl+letter combos */
                    switch(LINEDIT_KEYPRESSGETCHAR(&key)) {
                        case 'A': /* Move to start of line */
                        {
                            linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                            
                            int line;
                            linedit_stringcoordinates(&edit->current, edit->posn, NULL, &line);
                            linedit_stringfindposition(&edit->current, 0, line, &edit->posn);
                        }
                            break;
                        case 'B': /* Move backward */
                            linedit_processarrowkeypress(edit, LINEDIT_DEFAULTMODE, -1);
                            break;
                        case 'C': /* Copy */
                            if (linedit_getmode(edit)==LINEDIT_SELECTIONMODE) {
                                int lposn=(edit->sposn < edit->posn ? edit->sposn : edit->posn);
                                int rposn = (edit->sposn < edit->posn ? edit->posn : edit->sposn);
                                size_t lindx, rindx;
                                if (!linedit_stringutf8index(&edit->current, lposn, 0, &lindx)) break;
                                if (!linedit_stringutf8index(&edit->current, rposn, 0, &rindx)) break;
                                linedit_stringclear(&edit->clipboard);
                                linedit_stringappend(&edit->clipboard, edit->current.string+lindx, (size_t) rindx-lindx);
                            }
                            break;
                        case 'D': /* Delete the character underneath the cursor */
                            linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                            linedit_stringdelete(&edit->current, edit->posn, 1);
                            break;
                        case 'E': { /* Move to end of line */
                            linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                            int line;
                            linedit_stringcoordinates(&edit->current, edit->posn, NULL, &line);
                            linedit_stringfindposition(&edit->current, -1, line, &edit->posn);
                        }
                            break;
                        case 'F': /* Move forward */
                            linedit_processarrowkeypress(edit, LINEDIT_DEFAULTMODE, +1);
                            break;
                        case 'G': { /* Abort current editing session */
                            linedit_stringclear(&edit->current);
                            edit->posn=0;
                            return false;
                        }
                        case 'L': /* Clear buffer */
                            linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                            linedit_stringclear(&edit->current);
                            edit->posn=0;
                            break;
                        case 'N': /* Next line */
                            linedit_processchangeline(edit, 1);
                            break;
                        case 'P': /* Previous line */
                            linedit_processchangeline(edit, -1);
                            break;
                        case 'V': /* Paste */
                            linedit_setmode(edit, LINEDIT_DEFAULTMODE);
                            if (edit->clipboard.length>0) {
                                linedit_stringinsert(&edit->current, edit->posn, edit->clipboard.string, edit->clipboard.length);
                                linedit_advanceposition(edit, linedit_stringlength(&edit->clipboard));
                            }
                            break;
                        default: break;
                    }
                    break;
                default:
                    break;
            }
        }
    } while (linedit_keypressavailable());
    
    if (regeneratesuggestions) linedit_generatesuggestions(edit);
    
    return true;
}

/* ----------------------------------------
 * Main loops for different terminal types
 * ---------------------------------------- */

/** If we're not attached to a terminal, e.g. a pipe, simply read the
    file in. */
void linedit_noterminal(lineditor *edit) {
    int c;
    linedit_stringclear(&edit->current);
    do {
        c = fgetc(stdin);
        if (c==EOF || c=='\n') return;
        char a = (char) c;
        linedit_stringappend(&edit->current, &a, 1);
    } while (true);
}

/** If the terminal is unsupported, default to fgets with a fixed buffer */
#define LINEDIT_UNSUPPORTEDBUFFER   4096
void linedit_unsupported(lineditor *edit) {
    char buffer[LINEDIT_UNSUPPORTEDBUFFER];
    printf("%s",edit->prompt.string);
    if (fgets(buffer, LINEDIT_UNSUPPORTEDBUFFER, stdin)==buffer) {
        int length=(int) strlen(buffer);
        if (length>0) for (length--; length>=0 && iscntrl(buffer[length]); length--) {
            buffer[length]='\0'; /* Remove trailing ctrl chars */
        }
        linedit_stringaddcstring(&edit->current, buffer);
    }
}

/** Normal interface used if terminal is present */
void linedit_supported(lineditor *edit) {
    linedit_enablerawmode();

    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
    linedit_getterminalwidth(edit);
    linedit_setposition(edit, 0);
    linedit_redraw(edit);
    
    int vpos=0, nlines=0; // Keep track of the current vertical position and line number

    while (linedit_processkeypress(edit)) {
        int newvpos;
        linedit_stringcoordinates(&edit->current, edit->posn, NULL, &newvpos);
        int newnlines=linedit_stringcountlines(&edit->current);
        linedit_changeheight(edit, nlines, newnlines, vpos, newvpos);
            
        linedit_redraw(edit);
        vpos=newvpos; nlines=newnlines;
    }

    /* Ensure we're always on the last line of the input when redrawing before exit */
    linedit_movetoend(edit);
    
    /* Remove any dangling suggestions */
    linedit_stringlistclear(&edit->suggestions);
    linedit_setmode(edit, LINEDIT_DEFAULTMODE);
    linedit_redraw(edit);
    
    linedit_disablerawmode();
    
    if (edit->current.length>0) {
        linedit_historyadd(edit, edit->current.string);
    }
    
    linedit_linefeed(); // Move to next line
}

/* **********************************************************************
 * Public interface
 * ********************************************************************** */

/** Initialize a line editor */
void linedit_init(lineditor *edit) {
    if (!edit) return;
    edit->color=NULL;
    edit->ncols=0;
    linedit_stringlistinit(&edit->history);
    linedit_stringlistinit(&edit->suggestions);
    edit->mode=LINEDIT_DEFAULTMODE;
    linedit_stringinit(&edit->current);
    linedit_stringinit(&edit->prompt);
    linedit_stringinit(&edit->cprompt);
    linedit_stringinit(&edit->clipboard);
    linedit_setprompt(edit, LINEDIT_DEFAULTPROMPT);
    edit->completer=NULL;
    edit->cref=NULL;
    edit->multiline=NULL;
    edit->mlref=NULL;
    edit->graphemefn=NULL;
    linedit_graphemeinit(&edit->graphemedict);
}

/** Finalize a line editor */
void linedit_clear(lineditor *edit) {
    if (!edit) return;
    if (edit->color) {
        free(edit->color);
        edit->color=NULL;
    }
    linedit_historyclear(edit);
    linedit_stringlistclear(&edit->suggestions);
    linedit_stringclear(&edit->current);
    linedit_stringclear(&edit->prompt);
    linedit_stringclear(&edit->cprompt);
    linedit_stringclear(&edit->clipboard);
    linedit_graphemeclear(&edit->graphemedict);
}

/** Public interface to the line editor.
 *  @param   edit - a line editor that has been initialized with linedit_init.
 *  @returns the string input by the user, or NULL if nothing entered. */
char *linedit(lineditor *edit) {
    if (!edit) return NULL; /** Ensure we are not passed a NULL pointer */
    
    linedit_stringclear(&edit->current);
    
    switch (linedit_checksupport()) {
        case LINEDIT_NOTTTY: linedit_noterminal(edit); break;
        case LINEDIT_UNSUPPORTED: linedit_unsupported(edit); break;
        case LINEDIT_SUPPORTED: linedit_supported(edit); break;
    }
    
    return linedit_cstring(&edit->current);
}

/** @brief Configures syntax coloring
 *  @param[in] edit             Line editor to configure
 *  @param[in] tokenizer  Callback function that will identify the next token from a string
 *  @param[in] ref               Reference that will be passed to the tokenizer callback function.
 *  @param[in] map             Map from token types to colors */
void linedit_syntaxcolor(lineditor *edit, linedit_tokenizefn tokenizer, void *ref, linedit_colormap *map) {
    if (!edit) return;
    if (!map) return;
    if (edit->color) free(edit->color);
    int ncols;
    
    for (ncols=0; map[ncols].type!=LINEDIT_ENDCOLORMAP; ncols++);
    
    edit->color = malloc(sizeof(linedit_syntaxcolordata)+ncols*sizeof(linedit_colormap));
    
    if (!edit->color) return;
    
    edit->color->tokenizer=tokenizer;
    edit->color->tokref=ref;
    edit->color->ncols=ncols;
    edit->color->lexwarning=false;
    for (unsigned int i=0; i<ncols; i++) {
        edit->color->col[i]=map[i];
    }
    
    qsort(edit->color->col, ncols, sizeof(linedit_colormap), linedit_colormapcmp);
}

/** @brief Configures autocomplete
 *  @param[in] edit               Line editor to configure
 *  @param[in] completer    Callback function that will identify autocomplete suggestions
 *  @param[in] ref                 Reference that will be passed to the autocomplete callback function. */
void linedit_autocomplete(lineditor *edit, linedit_completefn completer, void *ref) {
    if (!edit) return;
    edit->completer=completer;
    edit->cref=ref;
}

/** @brief Configures multiline editing
 *  @param[in] edit              Line editor to configure
 *  @param[in] multiline   Callback function to test whether to enter multiline mode
 *  @param[in] ref                 Reference that will be passed to the multiline callback function.
 *  @param[in] cprompt        Continuation prompt, or NULL to just reuse the regular prompt */
void linedit_multiline(lineditor *edit, linedit_multilinefn multiline, void *ref, char *cprompt) {
    edit->multiline=multiline;
    edit->mlref=ref; 
    linedit_stringclear(&edit->cprompt);
    if (cprompt) {
        linedit_stringaddcstring(&edit->cprompt, cprompt);
    } else {
        linedit_stringaddcstring(&edit->cprompt, edit->prompt.string);
    }
}

/** @brief Adds a completion suggestion
 *  @param completion   completion data structure
 *  @param string       string to add */
void linedit_addsuggestion(linedit_stringlist *completion, char *string) {
    linedit_stringlistadd(completion, string);
}

/** @brief Sets the prompt
 *  @param edit         Line editor to configure
 *  @param prompt       prompt string to use */
void linedit_setprompt(lineditor *edit, char *prompt) {
    if (!edit) return;
    linedit_stringclear(&edit->prompt);
    linedit_stringaddcstring(&edit->prompt, prompt);
}

/** @brief Sets the grapheme splitter to use
 *  @param[in] edit           Line editor to configure
 *  @param[in] graphemefn       Grapheme splitter to use */
void linedit_setgraphemesplitter(lineditor *edit, linedit_graphemefn graphemefn) {
    if (!edit) return;
    edit->graphemefn=graphemefn;
}

/** @brief Displays a string with a given color and emphasis
 *  @param edit         Line editor in use
 *  @param string       String to display */
void linedit_displaywithstyle(lineditor *edit, char *string, linedit_color col, linedit_emphasis emph) {
    if (linedit_checksupport()==LINEDIT_SUPPORTED) {
        linedit_string out;
        linedit_stringinit(&out);
        linedit_stringsetcolor(&out, col);
        linedit_stringsetemphasis(&out, emph);
        linedit_stringaddcstring(&out, string);
        linedit_stringdefaulttext(&out);
        
        printf("%s", out.string);
        
        linedit_stringclear(&out);
    } else {
        printf("%s", string);
    }
}

/** @brief Displays a string with syntax coloring
 *  @param edit         Line editor in use
 *  @param string       String to display
 */
void linedit_displaywithsyntaxcoloring(lineditor *edit, char *string) {
    if (linedit_checksupport()==LINEDIT_SUPPORTED) {
        linedit_string in, out;
        linedit_stringinit(&in);
        linedit_stringinit(&out);
        linedit_stringaddcstring(&in, string);
        
        linedit_syntaxcolorstring(edit, &in, &out);
        linedit_stringdefaulttext(&out);
        printf("%s", out.string);
        
        linedit_stringclear(&in);
        linedit_stringclear(&out);
    } else {
        printf("%s", string);
    }
}

/** @brief Gets the terminal width
 *  @param edit         Line editor in use
 *  @returns The width in characters */
int linedit_getwidth(lineditor *edit) {
    linedit_getterminalwidth(edit);
    return edit->ncols;
}

/** @brief Checks whether the underlying terminal is a TTY 
 *  @param[in] edit         Line editor to use
 *  @returns true if stdin and stdout are ttys */
bool linedit_checktty(void) {
    return linedit_checksupport()!=LINEDIT_NOTTTY;
}
