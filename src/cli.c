/** @file cli.c
 *  @author T J Atherton
 *
 *  @brief Command line interface
*/

#include <time.h>
#include "cli.h"
#include "parse.h"
#include "file.h"

#define CLI_BUFFERSIZE 1024

#define BLU   "\x1B[34m"
#define CYN   "\x1B[36m"
#define GRY   "\x1B[38;2;128;128;128m"
#define RESET "\x1B[0m"

/** Displays several strings with a specified style using linedit */
void cli_displaywithstyle(lineditor *edit, linedit_color col, linedit_emphasis emph, int n, ...) {
    va_list args;
    va_start(args, n);
    for (int i=0; i<n; i++) {
        char *str = va_arg(args, char *);
        linedit_displaywithstyle(edit, str, col, emph);
    }
    va_end(args);
}

/** Report an error if one has occurred. */
void cli_reporterror(error *err, vm *v) {
    lineditor linedit;
    linedit_init(&linedit);
    
    if (err->cat!=ERROR_NONE) {
        cli_displaywithstyle(&linedit, CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, "Error '", err->id, "'");
        
        if (ERROR_ISRUNTIMEERROR(*err)) {
            cli_displaywithstyle(&linedit, CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, ": ", err->msg, "\n");
            morpho_stacktrace(v);
        } else {
            if (err->line!=ERROR_POSNUNIDENTIFIABLE && err->posn!=ERROR_POSNUNIDENTIFIABLE) {
                char posnbuffer[CLI_BUFFERSIZE];
                snprintf(posnbuffer, CLI_BUFFERSIZE, " [line %u char %u", err->line, err->posn);
                linedit_displaywithstyle(&linedit, posnbuffer, CLI_ERRORCOLOR, CLI_NOEMPHASIS);
                
                if (err->module) {
                    cli_displaywithstyle(&linedit, CLI_ERRORCOLOR, CLI_NOEMPHASIS, 2, " in module ", err->module);
                }
                
                linedit_displaywithstyle(&linedit, "] ", CLI_ERRORCOLOR, CLI_NOEMPHASIS);
            }
            
            cli_displaywithstyle(&linedit, CLI_ERRORCOLOR, CLI_NOEMPHASIS, 3, ": ", err->msg, "\n");
        }
    }
    
    linedit_clear(&linedit);
}

/* **********************************************************************
 * CLI callbacks
 * ********************************************************************** */

/** Print callback */
void cli_printcallbackfn(vm *v, void *ref, char *string) {
    lineditor *linedit = (lineditor *) ref;

    cli_displaywithstyle(linedit, CLI_DEFAULTCOLOR, LINEDIT_BOLD, 1, string);
}

/** Warning callback */
void cli_warningcallbackfn(vm *v, void *ref, error *err) {
    lineditor *linedit = (lineditor *) ref;
    
    linedit_color col = (err->cat==ERROR_INFO ? CLI_INFOCOLOR : CLI_WARNINGCOLOR);
    char *type = (err->cat==ERROR_INFO ? "Information" : "Warning");
    
    cli_displaywithstyle(linedit, col, CLI_NOEMPHASIS, 6, type, " '", err->id, "': ", err->msg, "\n");
}

/* **********************************************************************
 * Interactive cli
 * ********************************************************************** */

const char *cli_file;

/** Define colors for different token types */
linedit_colormap cli_tokencolors[] = {
    { TOKEN_NEWLINE,            LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_QUESTION,           LINEDIT_YELLOW       },
    
    { TOKEN_STRING,             LINEDIT_BLUE         },
    { TOKEN_INTERPOLATION,      LINEDIT_BLUE         },
    { TOKEN_INTEGER,            LINEDIT_BLUE         },
    { TOKEN_NUMBER,             LINEDIT_BLUE         },
    { TOKEN_SYMBOL,             LINEDIT_CYAN         },
    
    { TOKEN_LEFTPAREN,          LINEDIT_DEFAULTCOLOR },
    { TOKEN_RIGHTPAREN,         LINEDIT_DEFAULTCOLOR },
    { TOKEN_LEFTSQBRACKET,      LINEDIT_DEFAULTCOLOR },
    { TOKEN_RIGHTSQBRACKET,     LINEDIT_DEFAULTCOLOR },
    { TOKEN_LEFTCURLYBRACKET,   LINEDIT_DEFAULTCOLOR },
    { TOKEN_RIGHTCURLYBRACKET,  LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_COLON,              LINEDIT_DEFAULTCOLOR },
    { TOKEN_SEMICOLON,          LINEDIT_DEFAULTCOLOR },
    { TOKEN_COMMA,              LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_PLUS,               LINEDIT_DEFAULTCOLOR },
    { TOKEN_MINUS,              LINEDIT_DEFAULTCOLOR },
    { TOKEN_STAR,               LINEDIT_DEFAULTCOLOR },
    { TOKEN_SLASH,              LINEDIT_DEFAULTCOLOR },
    { TOKEN_CIRCUMFLEX,         LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_PLUSPLUS,           LINEDIT_DEFAULTCOLOR },
    { TOKEN_MINUSMINUS,         LINEDIT_DEFAULTCOLOR },
    { TOKEN_PLUSEQ,             LINEDIT_DEFAULTCOLOR },
    { TOKEN_MINUSEQ,            LINEDIT_DEFAULTCOLOR },
    { TOKEN_STAREQ,             LINEDIT_DEFAULTCOLOR },
    { TOKEN_SLASHEQ,            LINEDIT_DEFAULTCOLOR },
    { TOKEN_HASH,               LINEDIT_DEFAULTCOLOR },
    { TOKEN_AT,                 LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_DOT,                LINEDIT_DEFAULTCOLOR },
    { TOKEN_DOTDOT,             LINEDIT_DEFAULTCOLOR },
    { TOKEN_DOTDOTDOT,          LINEDIT_DEFAULTCOLOR },
    { TOKEN_EXCLAMATION,        LINEDIT_DEFAULTCOLOR },
    
    { TOKEN_AMP,                LINEDIT_DEFAULTCOLOR },
    { TOKEN_VBAR,               LINEDIT_DEFAULTCOLOR },
    { TOKEN_DBLAMP,             LINEDIT_DEFAULTCOLOR },
    { TOKEN_DBLVBAR,            LINEDIT_DEFAULTCOLOR },
    { TOKEN_EQUAL,              LINEDIT_DEFAULTCOLOR },
    { TOKEN_EQ,                 LINEDIT_DEFAULTCOLOR },
    { TOKEN_NEQ,                LINEDIT_DEFAULTCOLOR },
    { TOKEN_LT,                 LINEDIT_DEFAULTCOLOR },
    { TOKEN_GT,                 LINEDIT_DEFAULTCOLOR },
    { TOKEN_LTEQ,               LINEDIT_DEFAULTCOLOR },
    { TOKEN_GTEQ,               LINEDIT_DEFAULTCOLOR },
    
    
    { TOKEN_TRUE,               LINEDIT_MAGENTA      },
    { TOKEN_FALSE,              LINEDIT_MAGENTA      },
    { TOKEN_NIL,                LINEDIT_MAGENTA      },
    { TOKEN_SELF,               LINEDIT_MAGENTA      },
    { TOKEN_SUPER,              LINEDIT_MAGENTA      },
    { TOKEN_IMAG,               LINEDIT_BLUE         },
    
    { TOKEN_PRINT,              LINEDIT_MAGENTA      },
    { TOKEN_VAR,                LINEDIT_MAGENTA      },
    { TOKEN_IF,                 LINEDIT_MAGENTA      },
    { TOKEN_ELSE,               LINEDIT_MAGENTA      },
    { TOKEN_IN,                 LINEDIT_MAGENTA      },
    { TOKEN_WHILE,              LINEDIT_MAGENTA      },
    { TOKEN_FOR,                LINEDIT_MAGENTA      },
    { TOKEN_DO,                 LINEDIT_MAGENTA      },
    { TOKEN_BREAK,              LINEDIT_MAGENTA      },
    { TOKEN_CONTINUE,           LINEDIT_MAGENTA      },
    { TOKEN_FUNCTION,           LINEDIT_MAGENTA      },
    { TOKEN_RETURN,             LINEDIT_MAGENTA      },
    { TOKEN_CLASS,              LINEDIT_MAGENTA      },
    { TOKEN_IMPORT,             LINEDIT_MAGENTA      },
    { TOKEN_AS,                 LINEDIT_MAGENTA      },
    { TOKEN_IS,                 LINEDIT_MAGENTA      },
    { TOKEN_WITH,               LINEDIT_MAGENTA      },
    { TOKEN_TRY,                LINEDIT_MAGENTA      },
    { TOKEN_CATCH,              LINEDIT_MAGENTA      },
    
    { TOKEN_SHEBANG,            LINEDIT_DEFAULTCOLOR },
    { TOKEN_INCOMPLETE,         LINEDIT_DEFAULTCOLOR },
    { TOKEN_EOF,                LINEDIT_DEFAULTCOLOR },
    { LINEDIT_ENDCOLORMAP,      LINEDIT_DEFAULTCOLOR }
};

/** A tokenizer for syntax coloring that uses the morpho lexer */
bool cli_lex(char *in, void *ref, linedit_token *out) {
    bool success=false;
    lexer *l=(lexer *) ref;
    if (!l) return false;
    lex_init(l, in, 0);
    
    token tok;
    error err;
    error_init(&err);
    
    if (lex(l, &tok, &err)) {
        out->start=(char *) tok.start;
        out->length=tok.length;
        out->type=(linedit_tokentype) tok.type;
        success=(tok.type!=TOKEN_EOF);
    }
    
    lex_clear(l);
    
    return success;
}

/** Autocomplete function */
bool cli_complete(char *in, void *ref, linedit_stringlist *c) {
    size_t len=strlen(in);
    
    /* First find the last token in the input */
    char *tok = in+len;
    /* Scan backwards from end of string over alphanumeric tokens */
    while (tok>in && !isspace(*(tok-1))) tok--;
    
    /* Ensure we have at least one character */
    if (iscntrl(*tok)) return false;
    
    /* Now try to match the token against a library of words */
    len=strlen(tok);
    char *words[] = {"as", "and", "break", "class", "continue", "do", "else", "for", "false", "fn", "help", "if", "in", "import", "nil", "or", "print", "return", "true", "var", "while", "quit", "self", "super", "this", "try", "catch", NULL};
    int success=false;
    
    for (unsigned int i=0; words[i]!=NULL; i++) {
        if ( (len<strlen(words[i])) &&
             (strncmp(tok, words[i], len)==0)) {
            linedit_addsuggestion(c, words[i]+len);
            success=true;
        }
    }
    
    return success;
}

/** Multiline function */
bool cli_multiline(char *in, void *ref) {
    int nb=0; 

    for (char *c=in; *c!='\0'; c++) {
        switch (*c) {
            case '(': nb+=1; break; 
            case ')': nb-=1; break;
            case '{': nb+=1; break;
            case '}': nb-=1; break;
            case '[': nb+=1; break;
            case ']': nb-=1; break;
            default: break; 
        }
    }

    return (nb>0);
}

/** Interactive help */
void cli_help (lineditor *edit, char *query, error *err, bool avail) {
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
    } else {
        while (isspace(*q) && *q!='\0') q++;
        printf("No help found for '%s'\n", q);
    }
}

/** @brief Provide a command line interface */
void cli(clioptions opt) {
    version morphoversion;
    morpho_version(&morphoversion);
    char morphoversionstring[VERSION_MAXSTRINGLENGTH];
    version_tostring(&morphoversion, VERSION_MAXSTRINGLENGTH, morphoversionstring);
    
    #ifdef MORPHO_LONG_BANNER
        // Original ASCII art source - https://www.asciiart.eu/animals/insects/butterflies
        printf(BLU " ___   ___ \n" RESET);
        printf(BLU "(" CYN " @ " GRY"\\Y/" CYN " @ " BLU ") " RESET "  |  morpho %s  | \U0001F44B Type 'help' or '?' for help\n", morphoversionstring);
        printf(BLU " \\" CYN"__" GRY"+|+" CYN"__" BLU"/  " RESET "  |  Documentation: https://morpho-lang.readthedocs.io/en/latest/ \n");
        printf(BLU"  {" CYN"_" BLU "/ \\" CYN "_" BLU"}   " RESET "  |  Code: https://github.com/Morpho-lang/morpho \n\n");
    #else
        printf("\U0001F98B morpho %s | \U0001F44B Type 'help' or '?' for help\n", morphoversionstring);
    #endif
    cli_file=NULL;
    
    /* Set up program and compiler */
    program *p = morpho_newprogram();
    compiler *c = morpho_newcompiler(p);
    
    bool help = help_initialize();
    
    /* Set up VM */
    vm *v = morpho_newvm();
    
    /* Line editor */
    lineditor edit;
    lexer l;
    linedit_init(&edit);
    linedit_setprompt(&edit, CLI_PROMPT);
    linedit_syntaxcolor(&edit, cli_lex, &l, cli_tokencolors);
    linedit_multiline(&edit, cli_multiline, NULL, CLI_CONTINUATIONPROMPT);
    linedit_autocomplete(&edit, cli_complete, NULL);

    morpho_setprintfn(v, cli_printcallbackfn, &edit);
    morpho_setwarningfn(v, cli_warningcallbackfn, &edit);
    
    error err; /* Error structure that received messages from the compiler and VM */
    bool success=false; /* Keep track of whether compilation and execution was successful */
    
    /* Initialize the error struct */
    error_init(&err);
    
    /* Read-evaluate-print loop */
    for (;;) {
        char *input=NULL;
        
        while (!input) input=linedit(&edit);
        
        /* Check for CLI commands. */
        /* Let the user quit by typing 'quit'. */
        if (strncmp(input, CLI_QUIT, 4)==0 && white_space_remainder(input,4)) {
			break;
        } else if (strncmp(input, CLI_HELP, strlen(CLI_HELP))==0) {
            cli_help(&edit, input+strlen(CLI_HELP), &err, help); continue;
        } else if (strncmp(input, CLI_SHORT_HELP, strlen(CLI_SHORT_HELP))==0) {
            cli_help(&edit, input+strlen(CLI_SHORT_HELP), &err, help); continue;
        }
        
        /* Compile code */
        success=morpho_compile(input, c, false, &err);
        
        if (success) {
            /* If compilation was successful, and we're in interactive mode, execute... */
            if (opt & CLI_DISASSEMBLE) {
                morpho_disassemble(p, NULL);
            }
            if (opt & CLI_RUN) {
                success=morpho_debug(v, p);
                if (!success) {
                    cli_reporterror(morpho_geterror(v), v);
                    err=*morpho_geterror(v);
                }
            }
        } else {
            /* ... otherwise just raise an error. */
            cli_reporterror(&err, v);
        }
    }
    
    linedit_clear(&edit);
    morpho_freevm(v);
    
    help_finalize();
    
    morpho_freecompiler(c);
    morpho_freeprogram(p);
}

/* **********************************************************************
 * Run a file
 * ********************************************************************** */

/** Loads and runs a file. */
void cli_run(const char *in, clioptions opt) {
    program *p = morpho_newprogram();
    compiler *c = morpho_newcompiler(p);
    vm *v = morpho_newvm();
    
    char *src = cli_loadsource(in);
    
    error err; /* Error structure that received messages from the compiler and VM */
    bool success=false; /* Keep track of whether compilation and execution was successful */
    
    /* Open the input file if provided */
    cli_file = in;
    file_setworkingdirectory(in);
    
    if (src) {
        /* Compile code */
        success=morpho_compile(src, c, (opt & CLI_OPTIMIZE), &err);
        
        /* Run code if successful */
        if (success) {
            if (opt & CLI_DISASSEMBLE) {
                if (opt & CLI_DISASSEMBLESHOWSRC) {
                    cli_disassemblewithsrc(p, src);
                } else {
                    morpho_disassemble(p, NULL);
                }
            }
            if (opt & CLI_RUN) {
                if (opt & CLI_DEBUG) {
                    success=morpho_debug(v, p);
                } else if (opt & CLI_PROFILE) {
                    success=morpho_profile(v, p);
                } else {
                    success=morpho_run(v, p);
                }
                if (!success) cli_reporterror(morpho_geterror(v), v);
            }
        } else {
            cli_reporterror(&err, v);
        }
    } else {
        printf("Could not open file '%s'.\n", in);
    }
    
    MORPHO_FREE(src);
    morpho_freevm(v);
    morpho_freeprogram(p);
    morpho_freecompiler(c);
}

/* **********************************************************************
 * Load source code
 * ********************************************************************** */

/** Loads a source file, returning it as a C-string. Call MORPHO_FREE on it when finished. */
char *cli_loadsource(const char *in) {
    const char *inn = (in ? in : cli_file);
    FILE *f = NULL; /* Input file */
    int size=0;
    
    varray_char buffer;
    varray_charinit(&buffer);
    
    /* Open the input file if provided */
    if (inn) f=file_openrelative(inn,"r"); // Try opening relative to the working directory
    if (!f) f=fopen(inn, "r"); 
    if (inn && !f) {
        return NULL;
    }
    
    /* Determine the file size */
    fseek(f, 0L, SEEK_END);
    size = ((int) ftell(f))+1;
    rewind(f);
    
    if (size) {
        /* Size the buffer to match */
        if (!varray_charresize(&buffer, size+1)) {
            return NULL;
        }
        
        /* Read in the file */
        for (char *c=buffer.data; !feof(f); c=c+strlen(c)) {
            if (!fgets(c, (int) (buffer.data+buffer.capacity-c), f)) { c[0]='\0'; break; }
        }
        
        fclose(f);
    }
    
    return buffer.data;
}

/* **********************************************************************
 * Source listing and disassembly
 * ********************************************************************** */

/** Displays a single line of source */
static void cli_printline(lineditor *edit, int line, char *prompt, char *src, int length) {
    printf("%s %4u : ", prompt, line);
    /* Display the src line */
    char srcline[length];
    strncpy(srcline, src, length-1);
    srcline[length-1]='\0';
    linedit_displaywithsyntaxcoloring(edit, srcline);
    printf("\n");
}

/** Disassembles the program showing syntax colored lines of source */
void cli_disassemblewithsrc(program *p, char *src) {
    lineditor edit;
    linedit_init(&edit);
    lexer l;
    linedit_syntaxcolor(&edit, cli_lex, &l, cli_tokencolors);
    
    int line=1, length=0;
    for (unsigned int i=0; src[i]!='\0'; i++) {
        length++;
        if (src[i]=='\n' || src[i]=='\0') {
            cli_printline(&edit, line, ">>>", src+i-length+1, length);
            morpho_disassemble(p, &line);
            line++; length=0;
        }
    }
    
    linedit_clear(&edit);
}

/** Displays a source listing from source lines start to end */
void cli_list(const char *in, int start, int end) {
    char *src = cli_loadsource(in);
    lineditor edit;
    
    if (src) {
        linedit_init(&edit);
        lexer l;
        linedit_syntaxcolor(&edit, cli_lex, &l, cli_tokencolors);
        
        int line=1, length=0;
        for (unsigned int i=0; src[i]!='\0'; i++) {
            length++;
            if (src[i]=='\n' || src[i]=='\0') {
                if (line>=start && line <=end) cli_printline(&edit, line, "", src+i-length+1, length);
                line++;
                length=0;
            }
        }
        
        linedit_clear(&edit);
        MORPHO_FREE(src);
    }
}

