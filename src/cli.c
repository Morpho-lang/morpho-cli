/** @file cli.c
 *  @author T J Atherton
 *
 *  @brief Command line interface
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>

#include <morpho.h>
#include <parse.h>
#include <file.h>

#include "cli.h"
#include "debugger.h"

#ifdef MORPHO_INCLUDE_HELP
    #include <help.h>
#endif

#ifdef CLI_USELIBUNISTRING
    #include <unigbrk.h>
#endif

#ifdef CLI_USELIBGRAPHEME
    #include <grapheme.h>
#endif

char *cli_globalsrc=NULL;
static int s_cli_lastexitcode = EXIT_SUCCESS;
static clioptions s_cli_activeopt = 0;

void cli_setexitcode(int code) {
    s_cli_lastexitcode = code;
}

int cli_exitcode(void) {
    return s_cli_lastexitcode;
}

void cli_applyoptions(clioptions opt) {
    s_cli_activeopt = opt;
}

/** True when ANSI styling should be emitted. */
bool cli_usecolor(void) {
    if (s_cli_activeopt & CLI_NOCOLOR) return false;
    const char *nocolor = getenv("NO_COLOR");
    if (nocolor && *nocolor) return false;
    if (!inline_checkstdouttty()) return false;
    return inline_checksupported();
}

bool cli_showhints(void) {
    return !(s_cli_activeopt & CLI_NOHINTS);
}

#define CLI_BUFFERSIZE 4096

#define RESET      "\x1B[0m"
#define BOLD       "\x1B[1m"
#define ITALIC     "\x1B[3m"
#define UNDERLINE  "\x1B[4m"

/* **********************************************************************
 * Utility functions
 * ********************************************************************** */

void cli_emitemphasis(int emph) {
    switch (emph) {
        case CLI_NOEMPHASIS: inline_emit(RESET); break;
        case CLI_BOLD: inline_emit(BOLD); break;
        case CLI_UNDERLINE: inline_emit(UNDERLINE); break;
        case CLI_ITALIC: inline_emit(ITALIC); break;
        default: break;
    }
}

/** Displays several strings with a specified style */
void cli_displaywithstyle(int col, int emph, int n, ...) {
    va_list args;
    va_start(args, n);
    bool color = cli_usecolor();
    
    fflush(stdout);
    for (int i=0; i<n; i++) {
        char *str = va_arg(args, char *);
        if (color) cli_emitemphasis(emph);
        if (color) if (col != CLI_DEFAULTCOLOR) inline_emitcolor(col);
        inline_emit(str);
    }
    if (color) inline_emit(RESET);
    fflush(stdout);
    
    va_end(args);
}

/** Report an error if one has occurred. */
void cli_reporterror(error *err, vm *v) {
    if (err->cat!=ERROR_NONE) {
        cli_setexitcode(EXIT_FAILURE);
        cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, "Error '", err->id, "'");
        
        if (ERROR_ISRUNTIMEERROR(*err)) {
            cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, ": ", err->msg, "\n");
            morpho_stacktrace(v);
        } else {
            if (err->line!=ERROR_POSNUNIDENTIFIABLE && err->posn!=ERROR_POSNUNIDENTIFIABLE) {
                char posnbuffer[CLI_BUFFERSIZE];
                snprintf(posnbuffer, CLI_BUFFERSIZE, " [line %u char %u", err->line, err->posn+1);
                cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 1, posnbuffer);
                
                if (err->file) {
                    cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, " in module '", err->file, "'");
                }
                
                cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 1, "] ");
            }
            
            cli_displaywithstyle(CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, ": ", err->msg, "\n");
        }
#ifdef MORPHO_INCLUDE_HINT
        if (err->hnt) { // Display the hint attached to the error
            if (cli_showhints()) {
                cli_displaywithstyle(CLI_HINTCOLOR, CLI_NOEMPHASIS, 3, "Hint: ", err->hnt->msg, "\n");
            }
            hint_free(&err->hnt);
        }
#endif
    }
}


/** Interactive help. Returns true if help was found or a useful hint was shown. */
#ifndef MORPHO_INCLUDE_HELP
static bool cli_help(inline_editor *edit, char *query, error *err, bool avail) {
    (void)avail;
    char *q=query;
    if (help_querylength(q, NULL)==0) {
        if (err->cat!=ERROR_NONE) {
            q=err->id;
			error_clear(err);
        } else {
            q=HELP_INDEXPAGE;
        }
    }
    
    objecthelptopic *topic = help_search(q);
    if (topic) {
        help_display(edit, topic);
        return true;
    }
    while (isspace(*q) && *q!='\0') q++;
    printf("No help found for '%s'\n", q);
    return false;
}
#else
static bool cli_help(inline_editor *edit, char *query, error *err, bool avail) {
    (void)err; (void)avail;
    char *q = query;
    while (isspace(*q) && *q!='\0') q++; // Strip any leading space
    bool blank = (*q == '\0'); // Check for blank query
    if (blank) q = "help";

    bool found = false;
    help_topic topic;
    if (morpho_helpastopic(q, &topic)) {
        hlp_displaytopic(edit, &topic);
        found = true;
    } else {
        varray_char result;
        varray_charinit(&result);
        help_queryhint(q, &result);
        if (result.count > 0) printf("%s\n", result.data);
        else printf("No help found for '%s'\n", q);
        varray_charclear(&result);
    }

    /* Index page promises a topic list below; show it for blank or "help". */
    if (blank || strcmp(q, "help") == 0) {
        varray_value list;
        varray_valueinit(&list);
        morpho_helptopics(&list);
        hlp_displaytopiclist(edit, &list, HLP_TOPICS_HDR);
        varray_valueclear(&list);
        found = true;
    }
    return found;
}
#endif

/* **********************************************************************
 * Morpho callbacks
 * ********************************************************************** */

/** Print callback */
void cli_printcallbackfn(vm *v, void *ref, char *string) {
    cli_displaywithstyle(CLI_DEFAULTCOLOR, CLI_BOLD, 1, string);
}

/** Input callback */
void cli_inputcallbackfn(vm *v, void *ref, morphoinputmode mode, varray_char *str) {
    if (mode==MORPHO_INPUT_KEYPRESS) {
        int key = getchar();
        if (key!=EOF) varray_charwrite(str, (char) key);
    } else {
        inline_editor *line=inline_new("");
        char *out=inline_readline(line);
        if (out) varray_charadd(str, out, (int) strlen(out));
        free(out);
        inline_free(line);
    }
}

/** Warning callback */
void cli_warningcallbackfn(vm *v, void *ref, error *err) {
    (void)v; (void)ref;
    cli_displaywithstyle(CLI_WARNINGCOLOR, CLI_NOEMPHASIS, 5, "Warning '", err->id, "': ", err->msg, "\n");
}

/** Debugger callback */
void cli_debuggercallbackfn(vm *v, void *ref) {
    (void)ref;
    clidebugger_enter(v);
}

/* **********************************************************************
 * Inline callbacks
 * ********************************************************************** */

/** Define colors for different token types (256-color palette) */
int palette[] = {
    CLI_DEFAULTCOLOR,                    // 0 default
    INLINE_YELLOW,                       // 1 help
    INLINE_COLOR_ANSI216(0, 1, 5),       // 2 string/integer/number literals (darker blue, visible on both backgrounds)
    INLINE_COLOR_ANSI216(0, 3, 4),       // 3 symbol (darker cyan/teal, distinct from blue)
    INLINE_MAGENTA,                      // 4 keyword
    INLINE_GRAY_ANSI(12),                // 5 comment (mid-level gray)
    INLINE_COLOR_ANSI216(5, 3, 0),       // 6 operator (amber)
    CLI_ERRORCOLOR                       // 7 debug breakpoint marker @
};

tokentype help[] = { TOKEN_QUESTION };
tokentype literal[] = { TOKEN_STRING, TOKEN_INTERPOLATION, TOKEN_INTEGER, TOKEN_NUMBER, TOKEN_IMAG };
tokentype symbols[] = { TOKEN_SYMBOL };
tokentype keywords[] = { TOKEN_TRUE, TOKEN_FALSE, TOKEN_NIL, TOKEN_SELF, TOKEN_SUPER, TOKEN_PRINT, TOKEN_VAR, TOKEN_IF, TOKEN_ELSE, TOKEN_IN, TOKEN_WHILE, TOKEN_FOR, TOKEN_DO, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_FUNCTION,
    TOKEN_RETURN, TOKEN_CLASS, TOKEN_IMPORT, TOKEN_AS, TOKEN_IS, TOKEN_WITH, TOKEN_TRY, TOKEN_CATCH };
tokentype operators[] = {
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_CIRCUMFLEX,
    TOKEN_PLUSPLUS, TOKEN_MINUSMINUS,
    TOKEN_PLUSEQ, TOKEN_MINUSEQ, TOKEN_STAREQ, TOKEN_SLASHEQ,
    TOKEN_HASH,
    TOKEN_EXCLAMATION, TOKEN_AMP, TOKEN_VBAR, TOKEN_DBLAMP, TOKEN_DBLVBAR,
    TOKEN_EQUAL, TOKEN_EQ, TOKEN_NEQ,
    TOKEN_LT, TOKEN_GT, TOKEN_LTEQ, TOKEN_GTEQ,
    TOKEN_DOTDOT, TOKEN_DOTDOTDOT
};

/** Checks if match matches any tokentype in a given list */
static bool matchtokentype(tokentype match, size_t n, tokentype *list) {
    for (size_t i=0; i<n; i++) if (list[i]==match) return true;
    return false;
}

/** Detect and parse comments manually (lexer skips them).
 * Returns the byte position after the comment, or offset if no comment found. */
static size_t detect_comment(const char *in, size_t offset) {
    const char *start = in + offset;
    
    // Skip whitespace first
    const char *p = start;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    
    // Check for // style comment
    if (p[0] == '/' && p[1] == '/') {
        p += 2; 
        while (*p != '\0' && *p != '\n' && *p != '\r') p++; // Scan to end of line
        return p - in;
    }
    
    // Check for /* style comment
    if (p[0] == '/' && p[1] == '*') {
        p += 2;
        int nesting = 1; // Track nesting depth
        
        while (nesting > 0 && *p != '\0') {
            if (p[0] == '/' && p[1] == '*') {
                nesting++;
                p += 2;
            } else if (p[0] == '*' && p[1] == '/') {
                nesting--;
                p += 2;
            } else p++;
        }
        return p - in;
    }
    
    return offset; // No comment found
}

/** A tokenizer for syntax coloring that uses the morpho lexer */
bool cli_syntaxcolorfn(const char *in, void *ref, size_t offset, inline_colorspan_t *out) {
    bool success=false;
    lexer *l=(lexer *) ref;
    
    if (!l) return false;
    
    // Check for comments first (before lexer processing)
    size_t comment_end = detect_comment(in, offset);
    if (comment_end > offset) {
        // Found a comment
        out->color = 5; // Comment color (gray)
        out->byte_end = comment_end;
        return true; // Successfully colored a comment
    }
    
    if (offset==0) lex_init(l, in, 0);
    else l->current=in+offset; 

    token tok;
    error err;
    error_init(&err);
    
    if (lex(l, &tok, &err)) {
        out->color=0;
        if (tok.start>in+offset) { // Token began after offset
            out->byte_end=tok.start-in;
        } else { // A real token
            out->byte_end=offset+tok.length;
            if (tok.type==TOKEN_AT) out->color=7;  // Debug breakpoint marker in red
            else if (matchtokentype(tok.type, sizeof(help)/sizeof(help[0]), help)) out->color=1;
            else if (matchtokentype(tok.type, sizeof(literal)/sizeof(literal[0]), literal)) out->color=2;
            else if (matchtokentype(tok.type, sizeof(symbols)/sizeof(symbols[0]), symbols)) out->color=3;
            else if (matchtokentype(tok.type, sizeof(keywords)/sizeof(keywords[0]), keywords)) out->color=4;
            else if (matchtokentype(tok.type, sizeof(operators)/sizeof(operators[0]), operators)) out->color=6;
        }
        success=(tok.type!=TOKEN_EOF);
    }
    
    if (tok.type==TOKEN_EOF) lex_clear(l);
    error_clear(&err);
    
    return success;
}

static char *words[] = {"as", "and", "break", "catch", "class", "continue", "do", "else", "false", "fn", "for", "help", "if", "import", "in", "is", "nil", "or", "print", "quit", "return", "self", "super", "true", "try", "var", "while", "with", NULL};

/** Autocomplete function */
const char *cli_complete(const char *in, void *ref, size_t *index) {
    (void)ref;
    size_t len=strlen(in);
    
    /* First find the last token in the input */
    const char *tok = in+len;
    /* Scan backwards from end of string over alphanumeric tokens */
    while (tok>in && !isspace(*(tok-1))) tok--;
    
    /* Ensure we have at least one character */
    if (iscntrl(*tok)) return NULL;
    
    /* Now try to match the token against a library of words */
    len=strlen(tok);
    
    for (size_t i=*index; words[i]!=NULL; i++) {
        if ( (len<strlen(words[i])) &&
             (strncmp(tok, words[i], len)==0)) {
            *index=i+1;
            return words[i]+len;
        }
    }

    return NULL;
}

/** Multiline function */
bool cli_multiline(const char *in, void *ref) {
    int nb=0;
    const char *c;

    for (c=in; isspace(*c); c++); // Skip leading whitespace
    if (*c=='?' || strncmp(c, "help", 4)==0) return false; 

    for (; *c!='\0'; c++) {
        switch (*c) {
            case '(': case '{': case '[': nb+=1; break;
            case ')': case '}': case ']': nb-=1; break;
            default: break;
        }
    }

    return (nb>0);
}

#ifdef CLI_USELIBUNISTRING
size_t libunistring_graphemefn(const char *in, const char *end) {
    char *next = (char *) u8_grapheme_next((uint8_t *) in, (uint8_t *) end);
    if (next>in) return next-in;
    return 0;
}
#endif

#ifdef CLI_USELIBGRAPHEME
size_t libgrapheme_graphemefn(const char *in, const char *end) {
    return grapheme_next_character_break_utf8(in, end-in);
}
#endif

/* **********************************************************************
 * Interactive mode
 * ********************************************************************** */

/** Runtime context holding VM, compiler, program, editor, and lexer */
typedef struct {
    vm *v;
    compiler *c;
    program *p;
    inline_editor *edit;
    lexer l;
} runtime_t;

/** Forward declaration */
static void cli_repl(runtime_t *rt, clioptions opt);
static bool cli_compileandrun(runtime_t *rt, const char *src, clioptions opt);
static runtime_t cli_newruntime(clioptions opt);
static void cli_freeruntime(runtime_t *rt);

/** Compile preamble (if any) then src (if any) in an existing runtime.
 *  @returns false if a compile/run step failed. */
static bool cli_runpreambleandsource(runtime_t *rt, const char *preamble, const char *src, clioptions opt) {
    if (preamble && *preamble) {
        cli_globalsrc = (char *) preamble;
        if (!cli_compileandrun(rt, preamble, opt)) return false;
    }
    if (src) {
        cli_globalsrc = (char *) src;
        if (!cli_compileandrun(rt, src, opt)) return false;
    }
    return true;
}

/** @brief Provide a command line interface */
void cli(clioptions opt, const char *preamble) {
    runtime_t rt = cli_newruntime(opt);
    if (cli_runpreambleandsource(&rt, preamble, NULL, opt)) {
        cli_repl(&rt, opt);
    }
    cli_freeruntime(&rt);
}

/* **********************************************************************
 * Helper structures and functions
 * ********************************************************************** */

/** Set up runtime context (VM, compiler, program, editor, callbacks) */
static runtime_t cli_newruntime(clioptions opt) {
    runtime_t rt = { NULL, NULL, NULL, NULL };
    rt.p = morpho_newprogram();
    rt.c = morpho_newcompiler(rt.p);
    rt.v = morpho_newvm();
    rt.edit = inline_new(CLI_PROMPT);
    
    cli_applyoptions(opt);
    if (cli_usecolor()) {
        inline_syntaxcolor(rt.edit, cli_syntaxcolorfn, &rt.l);
        inline_setpalette(rt.edit, sizeof(palette)/sizeof(palette[0]), palette);
    }
    
    morpho_setinputfn(rt.v, cli_inputcallbackfn, NULL);
    morpho_setprintfn(rt.v, cli_printcallbackfn, &rt.edit);
    morpho_setwarningfn(rt.v, cli_warningcallbackfn, &rt.edit);
    morpho_setdebuggerfn(rt.v, cli_debuggercallbackfn, NULL);
    
    return rt;
}

/** Clean up runtime context */
static void cli_freeruntime(runtime_t *rt) {
    if (rt->edit) inline_free(rt->edit);
    if (rt->v) morpho_freevm(rt->v);
    if (rt->p) morpho_freeprogram(rt->p);
    if (rt->c) morpho_freecompiler(rt->c);
}

/** Run a compiled program according to CLI flags. */
static bool cli_execute(runtime_t *rt, clioptions opt, error *err_out) {
    if (!(opt & CLI_RUN)) return true;

    bool success;
    if (opt & CLI_DEBUG) {
        success = morpho_debug(rt->v, rt->p);
    } else if (opt & CLI_PROFILE) {
        success = morpho_profile(rt->v, rt->p);
    } else {
        success = morpho_run(rt->v, rt->p);
    }

    if (!success) {
        cli_reporterror(morpho_geterror(rt->v), rt->v);
        if (err_out) *err_out = *morpho_geterror(rt->v);
    }
    return success;
}

/** Compile and execute source code */
static bool cli_compileandrun(runtime_t *rt, const char *src, clioptions opt) {
    error err;
    error_init(&err);
    
    bool success = morpho_compile((char *)src, rt->c, (opt & CLI_OPTIMIZE) != 0, &err);
    
    if (success) {
        if (opt & CLI_DISASSEMBLE) {
            if (opt & CLI_DISASSEMBLESHOWSRC) {
                cli_disassemblewithsrc(rt->p, (char *)src, opt);
            } else {
                morpho_disassemble(rt->v, rt->p, NULL);
            }
        }
        success = cli_execute(rt, opt, NULL);
    } else {
        cli_reporterror(&err, rt->v);
    }
    
    return success;
}

/* **********************************************************************
 * Non-interactive run
 * ********************************************************************** */

/** Compile and run source string (no file). Used by -e / --eval. */
void cli_runstring(const char *src, clioptions opt, const char *preamble) {
    runtime_t rt = cli_newruntime(opt);
    
    if (cli_runpreambleandsource(&rt, preamble, src, opt) && (opt & CLI_INTERACTIVE)) {
        cli_repl(&rt, opt);
    }
    cli_freeruntime(&rt);
}

/** Enter REPL mode, reusing an existing runtime */
static void cli_repl(runtime_t *rt, clioptions opt) {
    bool tty = inline_checktty();
    version morphoversion;
    morpho_version(&morphoversion);
    char morphoversionstring[VERSION_MAXSTRINGLENGTH];
    version_tostring(&morphoversion, VERSION_MAXSTRINGLENGTH, morphoversionstring);
    
    if (tty) {
        inline_setutf8();
        printf("\U0001F98B morpho %s | \U0001F44B Type 'help' or '?' for help\n", morphoversionstring);
    }
    
    bool help = hlp_initialize();
    
    varray_char src;
    varray_charinit(&src);
    varray_charwrite(&src, '\0');
    
    /* Configure editor for REPL */
    if (cli_usecolor()) {
        inline_setpalette(rt->edit, sizeof(palette)/sizeof(palette[0]), palette);
        inline_syntaxcolor(rt->edit, cli_syntaxcolorfn, &rt->l);
    }
    inline_multiline(rt->edit, cli_multiline, NULL, CLI_CONTINUATIONPROMPT);
    inline_autocomplete(rt->edit, cli_complete, NULL);
#ifdef CLI_USELIBUNISTRING
    inline_setgraphemesplitter(rt->edit, libunistring_graphemefn);
#endif
#ifdef CLI_USELIBGRAPHEME
    inline_setgraphemesplitter(rt->edit, libgrapheme_graphemefn);
#endif
    
    error err;
    error_init(&err);
    
    for (int n = 0; ; n++) {
        if (!tty && n > 0) break;
        char *input = NULL;
        while (!input) input = inline_readline(rt->edit);
        
        if (strncmp(input, CLI_QUIT, strlen(CLI_QUIT)) == 0) {
            free(input);
            break;
        } else if (strncmp(input, CLI_HELP, strlen(CLI_HELP)) == 0) {
            cli_help(rt->edit, input + strlen(CLI_HELP), &err, help);
            free(input);
            continue;
        } else if (strncmp(input, CLI_SHORT_HELP, strlen(CLI_SHORT_HELP)) == 0) {
            cli_help(rt->edit, input + strlen(CLI_SHORT_HELP), &err, help);
            free(input);
            continue;
        }
        
        bool success = morpho_compile(input, rt->c, (opt & CLI_OPTIMIZE) != 0, &err);
        
        if (success) {
            src.count--;
            varray_charadd(&src, input, (int)strlen(input));
            varray_charwrite(&src, '\n');
            varray_charwrite(&src, '\0');
            cli_globalsrc = src.data;
            
            if (opt & CLI_DISASSEMBLE) {
                morpho_disassemble(rt->v, rt->p, NULL);
            }
            success = cli_execute(rt, opt, &err);
        } else {
            cli_reporterror(&err, rt->v);
        }
        
        free(input);
    }
    
    varray_charclear(&src);
    hlp_finalize();
}

/* **********************************************************************
 * Load and run a file
 * ********************************************************************** */

void cli_run(const char *in, clioptions opt, const char *preamble) {
    runtime_t rt = cli_newruntime(opt);
    
    char *src = cli_loadsource(in);
    
    file_setworkingdirectory(in);
    
    if (src) {
        if (cli_runpreambleandsource(&rt, preamble, src, opt) && (opt & CLI_INTERACTIVE)) {
            cli_repl(&rt, opt);
        }
        MORPHO_FREE(src);
    } else {
        printf("Could not open file '%s'.\n", in);
        cli_setexitcode(EXIT_FAILURE);
    }
    cli_freeruntime(&rt);
}

/* **********************************************************************
 * Load source code
 * ********************************************************************** */

/** Loads source from stdin, returning it as a C-string. Call MORPHO_FREE on it when finished. */
char *cli_loadstdin(void) {
    varray_char buffer;
    varray_charinit(&buffer);
    
    /* Read from stdin in chunks */
    char chunk[CLI_BUFFERSIZE];
    size_t nread;
    
    while ((nread = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
        varray_charadd(&buffer, chunk, (int)nread);
    }
    
    /* Ensure null termination */
    varray_charwrite(&buffer, '\0');
    
    return buffer.data;
}

/** Loads a source file, returning it as a C-string. Call MORPHO_FREE on it when finished. */
char *cli_loadsource(const char *in) {
    FILE *f = NULL; /* Input file */
    int size=0;
    
    varray_char buffer;
    varray_charinit(&buffer);
    
    /* Open the input file if provided */
    if (in) f=file_openrelative(in,"r"); // Try opening relative to the working directory
    if (!f) f=fopen(in, "r");
    if (!f) return NULL;
    
    /* Determine the file size */
    fseek(f, 0L, SEEK_END);
    size = ((int) ftell(f))+1;
    rewind(f);
    
    if (size) {
        /* Size the buffer to match */
        if (!varray_charresize(&buffer, size+1)) {
            fclose(f);
            return NULL;
        }
        
        /* Read in the file */
        for (char *c=buffer.data; !feof(f); c=c+strlen(c)) {
            if (!fgets(c, (int) (buffer.data+buffer.capacity-c), f)) { c[0]='\0'; break; }
        }
    }
    
    fclose(f);
    return buffer.data;
}

/* **********************************************************************
 * Source listing and disassembly
 * ********************************************************************** */

/** Create a temporary editor configured for colored display. */
static inline_editor *cli_tempeditor(lexer *l) {
    inline_editor *edit = inline_new("");
    if (!edit) return NULL;
    if (cli_usecolor()) {
        inline_syntaxcolor(edit, cli_syntaxcolorfn, l);
        inline_setpalette(edit, sizeof(palette)/sizeof(palette[0]), palette);
    }
    return edit;
}

/** Displays a single line of source. nbytes is the span length; a trailing newline is stripped. */
static void cli_printline(inline_editor *edit, int line, const char *prompt, const char *src, int nbytes) {
    printf("%s %4u : ", prompt, line);
    char stackbuf[512];
    char *buf = (nbytes + 1 <= (int) sizeof(stackbuf)) ? stackbuf : malloc((size_t) nbytes + 1);
    if (!buf) return;
    memcpy(buf, src, (size_t) nbytes);
    buf[nbytes] = '\0';
    if (nbytes > 0 && buf[nbytes - 1] == '\n') buf[nbytes - 1] = '\0';
    inline_displaywithsyntaxcoloring(edit, buf);
    printf("\n");
    if (buf != stackbuf) free(buf);
}

typedef void (*cli_sourcelinefn)(inline_editor *edit, int line, const char *start, int nbytes, void *ref);

/** Walk source lines in [startline, endline] and invoke fn for each. */
static void cli_foreachsourceline(const char *src, int startline, int endline,
                                  cli_sourcelinefn fn, void *ref) {
    if (!src || !fn) return;
    lexer l;
    inline_editor *edit = cli_tempeditor(&l);
    if (!edit) return;

    int line = 1, length = 0;
    for (unsigned int i = 0; src[i] != '\0'; i++) {
        length++;
        if (src[i] == '\n') {
            if (line >= startline && line <= endline)
                fn(edit, line, src + i - length + 1, length, ref);
            line++;
            length = 0;
        }
    }
    if (length > 0 && line >= startline && line <= endline)
        fn(edit, line, src + strlen(src) - length, length, ref);

    inline_free(edit);
}

static void cli_listline(inline_editor *edit, int line, const char *start, int nbytes, void *ref) {
    (void)ref;
    cli_printline(edit, line, "", start, nbytes);
}

static void cli_disassembleline(inline_editor *edit, int line, const char *start, int nbytes, void *ref) {
    program *p = (program *) ref;
    int matchline = line;
    cli_printline(edit, line, ">>>", start, nbytes);
    morpho_disassemble(NULL, p, &matchline);
}

/** Disassembles the program showing syntax colored lines of source */
void cli_disassemblewithsrc(program *p, char *src, clioptions opt) {
    clioptions prev = s_cli_activeopt;
    s_cli_activeopt |= opt;
    cli_foreachsourceline(src, 1, INT_MAX, cli_disassembleline, p);
    s_cli_activeopt = prev;
}

/** Displays a source listing from source lines start to end */
void cli_list(const char *src, int start, int end, clioptions opt) {
    clioptions prev = s_cli_activeopt;
    s_cli_activeopt |= opt;
    cli_foreachsourceline(src, start, end, cli_listline, NULL);
    s_cli_activeopt = prev;
}

/** Look up and display a help topic from the command line. */
void cli_helpquery(const char *query, clioptions opt) {
    cli_applyoptions(opt);
    lexer l;
    inline_editor *edit = cli_tempeditor(&l);
    if (!edit) {
        cli_setexitcode(EXIT_FAILURE);
        return;
    }

    error err;
    error_init(&err);
    bool help = hlp_initialize();
    char *q = query ? (char *) query : (char *) "";
    if (!cli_help(edit, q, &err, help)) cli_setexitcode(EXIT_FAILURE);
    hlp_finalize();
    error_clear(&err);
    inline_free(edit);
}

