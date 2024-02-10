/** @file linedit.h
 *  @author T J Atherton
 *
 *  @brief A simple UTF8 aware line editor with history, completion, multiline editing and syntax highlighting
*/

#ifndef linedit_h
#define linedit_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* **********************************************************************
 * Types
 * ********************************************************************** */

/* -----------------------
 * Linedit strings
 * ----------------------- */

typedef struct linedit_string_s linedit_string;

/** lineditor strings */
struct linedit_string_s {
    size_t capacity;  /** Capacity of the string in bytes */
    size_t length; /** Length in bytes */
    char *string; /** String data */
    linedit_string *next; /** Enable strings to be chained together */
} ;

/** A list of strings */
typedef struct {
    int posn; /* We use this to keep track of where in the list the user is */
    linedit_string *first;
} linedit_stringlist;

/* -----------------------
 * Tokenization
 * ----------------------- */

/** Token types */
typedef int linedit_tokentype;

/** lineditor tokens */
typedef struct {
    linedit_tokentype type;
    char *start;
    size_t length;
} linedit_token;

/** @brief Tokenizer callback function
 *  @param   in    - a string
 *  @param   ref   - pointer to a reference structure provided to linedit by the user
 *  @param   tok   - pointer to a token structure that the caller should fill out.
 *  @details This user function is called when linedit needs to tokenize a string.
 *           The function should identify the next token in the string and fill out
 *           the following fields:
 *             tok->type   - should contain the token type. This is used e.g.
 *                           an index to the color array.
 *             tok->start  - should point to the first significant character
 *                           in the token
 *             tok->length - should contain the length of the token, in bytes
 *           The function should return true if a token was successfully processed or
 *           false otherwise. */
typedef bool (*linedit_tokenizefn) (char *in, void *ref, linedit_token *tok);

/* -----------------------
 * Color
 * ----------------------- */

/** Colors */
typedef enum {
    LINEDIT_BLACK,
    LINEDIT_RED,
    LINEDIT_GREEN,
    LINEDIT_YELLOW,
    LINEDIT_BLUE,
    LINEDIT_MAGENTA,
    LINEDIT_CYAN,
    LINEDIT_WHITE,
    LINEDIT_DEFAULTCOLOR, 
} linedit_color;

typedef enum {
    LINEDIT_BOLD,
    LINEDIT_UNDERLINE,
    LINEDIT_REVERSE,
    LINEDIT_NONE
} linedit_emphasis;

#define LINEDIT_ENDCOLORMAP -1

typedef struct {
    linedit_tokentype type;
    linedit_color col;
} linedit_colormap;

/** Structure to hold all information related to syntax coloring */
typedef struct {
    linedit_tokenizefn tokenizer; /** A tokenizer function */
    void *tokref;                 /** Reference passed to tokenizer callback function */
    bool lexwarning;
    unsigned int ncols;           /** Number of colors provided */
    linedit_colormap col[];       /** Flexible array member mapping token types to colors */
} linedit_syntaxcolordata;

/* -----------------------
 * Completion
 * ----------------------- */

/** @brief Autocompletion callback function
 *  @param[in] in     - a string
 *  @param[in] ref   - pointer to a reference structure provided to linedit by the user
 *  @param[out] completion  - autocompletion structure
 *  @details This user function is called when linedit requests autocompletion
 *           of a string. The function should identify any possible suggestions
 *           and call linedit_addcompletion to add them one by one.
 *
 *           Only *remaining* characters from the suggestion should be added,
 *           e.g. for "hello" if the user has typed "he" the function should add
 *           "llo" as a suggestion.
 *
 *           The function should return true if autocompletion was successfully
 *           processed or false otherwise.
*/
typedef bool (*linedit_completefn) (char *in, void *ref, linedit_stringlist *completion);

/* -----------------------
 * Multiline callback
 * ----------------------- */

/** @brief Multiline callback function
 *  @param[in]  in          - a string
 *  @param[in]  ref        - pointer to a reference structure provided by the user
 *  @details This user function is called when linedit wants to know whether
 *           it should enter multiline mode. The function should parse the 
 *           input and return true if linedit should go to multiline mode or
 *           false otherwise. Typically, return true if the input is incomplete 
*/
typedef bool (*linedit_multilinefn) (char *in, void *ref);

/* -----------------------
 * Unicode support
 * ----------------------- */

/** @brief Unicode grapheme splitter
 *  @param[in]  in          - a string
 *  @param[in]  maxlength - maximum length of the grapheme [set to SIZE_MAX]
 *  @returns offset to next grapheme
 *  @details If provided, linedit will use this function to split UTF8 code into graphemes, which enables length estimation.
*/
typedef size_t (*linedit_graphemefn) (char *in, size_t maxlength);

/* -----------------------
 * lineditor structure
 * ----------------------- */

#define LINEDIT_DEFAULTPROMPT   ">"

/** Keep track of what the line editor is doing */
typedef enum {
    LINEDIT_DEFAULTMODE,
    LINEDIT_SELECTIONMODE,
    LINEDIT_HISTORYMODE
} lineditormode;

/** Holds all state information needed for a line editor */
typedef struct {
    lineditormode mode;      /** Current editing mode */
    int posn;                /** Position of the cursor in UTF8 characters */
    int sposn;               /** Starting point of a selection */
    int ncols;               /** Number of columns */
    linedit_string prompt;   /** The prompt */
    linedit_string cprompt;   /** Continuation prompt */
    
    linedit_string current;  /** Current string that's being edited */
    linedit_string clipboard;/** Copy/paste clipboard */
    
    linedit_stringlist history; /** History list */
    linedit_stringlist suggestions; /** Autocompletion suggestions */
    
    linedit_syntaxcolordata *color; /** Structure to handle syntax coloring */
    
    linedit_completefn completer; /** Autocompletion callback function*/
    void *cref;                   /** Reference for autocompletion callback function */
    
    linedit_multilinefn multiline; /** Multiline callback */
    void *mlref;                   /** Reference for multiline callback function */
    
    linedit_graphemefn graphemefn; /** Grapheme splitting */
} lineditor;

/* **********************************************************************
 * Public interface
 * ********************************************************************** */

/** Public interface to the line editor.
 *  @param[in] edit - A line editor that has been initialized with linedit_init.
 *  @returns the string input by the user, or NULL if nothing entered. */
char *linedit(lineditor *edit);

/** @brief Configures syntax coloring
 *  @param[in] edit             Line editor to configure
 *  @param[in] tokenizer  Callback function that will identify the next token from a string
 *  @param[in] ref               Reference that will be passed to the tokenizer callback function.
 *  @param[in] map               Map from token types to colors */
void linedit_syntaxcolor(lineditor *edit, linedit_tokenizefn tokenizer, void *ref, linedit_colormap *cols);

/** @brief Configures autocomplete
 *  @param[in] edit               Line editor to configure
 *  @param[in] completer    Callback function that will identify autocomplete suggestions
 *  @param[in] ref                 Reference that will be passed to the autocomplete callback function. */
void linedit_autocomplete(lineditor *edit, linedit_completefn completer, void *ref);

/** @brief Configures multiline editing
 *  @param[in] edit              Line editor to configure
 *  @param[in] multiline   Callback function to test whether to enter multiline mode
 *  @param[in] ref                 Reference that will be passed to the multiline callback function.
 *  @param[in] cprompt        Continuation prompt, or NULL to just reuse the regular prompt */
void linedit_multiline(lineditor *edit, linedit_multilinefn multiline, void *ref, char *cprompt);

/** @brief Adds a completion suggestion
 *  @param[in] completion   Completion data structure
 *  @param[in] string            String to add */
void linedit_addsuggestion(linedit_stringlist *completion, char *string);

/** @brief Sets the prompt
 *  @param[in] edit           Line editor to configure
 *  @param[in] prompt       Prompt string to use */
void linedit_setprompt(lineditor *edit, char *prompt);

/** @brief Sets the grapheme splitter to use
 *  @param[in] edit           Line editor to configure
 *  @param[in] graphemefn       Grapheme splitter to use */
void linedit_setgraphemesplitter(lineditor *edit, linedit_graphemefn graphemefn);

/** @brief Displays a string with a given color and emphasis
 *  @param[in] edit           Line editor to use
 *  @param[in] string      String to display
 *  @param[in] col             Color
 *  @param[in] emph           Emphasis */
void linedit_displaywithstyle(lineditor *edit, char *string, linedit_color col, linedit_emphasis emph);

/** @brief Displays a string with syntax coloring
 *  @param[in] edit           Line editor to use
 *  @param[in] string      String to display */
void linedit_displaywithsyntaxcoloring(lineditor *edit, char *string);

/** @brief Gets the terminal width
 *  @param[in] edit         Line editor to use
 *  @returns The width in characters */
int linedit_getwidth(lineditor *edit);

/** @brief Checks whether the underlying terminal is a TTY 
 *  @returns true if stdin and stdout are ttys */
bool linedit_checktty(void);

/** Initialize a line editor */
void linedit_init(lineditor *edit);

/** Finalize a line editor */
void linedit_clear(lineditor *edit);

#endif /* linedit_h */
