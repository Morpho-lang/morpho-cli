/** @file debugger.c
 *  @author T J Atherton
 *
 *  @brief Command line debugger
*/

#include <compile.h>
#include <vm.h>
#include <parse.h>
#include <debug.h>
#include <gc.h>

#include "debugger.h"

/** @brief Debugger front end
 *
 * @details The debugger front end is implemented using the new lexer and parser:
 * - we define all debugger commands and shortcuts as tokens
 * - debugger commands are implemented via parselets */

/* **********************************************************************
 * Debugger front end structure
 * ********************************************************************** */

typedef struct {
    debugger *debug; /** Debugger */
    lineditor *edit; /** lineeditor for output */
    error *err; /** Error structure to fill out  */
    char *info; /** Report any info to the user after error messages */
    bool stop;
} clidebugger;

void clidebugger_init(clidebugger *debug, vm *v, lineditor *edit, error *err) {
    debug->debug=vm_getdebugger(v);
    debugger_seterror(debug->debug, err);
    debug->edit=edit;
    debug->err=err;
    debug->info=NULL;
    debug->stop=false;
}

/* **********************************************************************
 * Debugger functions
 * ********************************************************************** */

/** Display the morpho banner */
void clidebugger_banner(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Morpho debugger---\n");
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1, "Type '?' or 'h' for help.\n");
    
    morpho_printf(debugger_currentvm(debug->debug), "%s ", (debug->debug->singlestep ? "Single stepping" : "Breakpoint"));
    debugger_showlocation(debug->debug, debug->debug->iindx);
    
    morpho_printf(debugger_currentvm(debug->debug), "\n");
}

/** Display the resume text */
void clidebugger_resumebanner(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Resuming----------\n");
}

/** Display an error message */
void clidebugger_reporterror(clidebugger *debug) {
    if (debug->err->cat!=ERROR_NONE) {
        cli_displaywithstyle(debug->edit, DEBUGGER_ERROR_COLOR, CLI_NOEMPHASIS, 3, "Error: ", debug->err->msg, "\n");
    }
}

/** Source listing */
void clidebugger_list(clidebugger *debug, int *nlines) {
    int line=0;
    value module=MORPHO_NIL;
    vm *v = debugger_currentvm(debug->debug);
    
    if (debug_infofromindx(v->current, vm_previnstruction(v), &module, &line, NULL, NULL, NULL)) {
        char *src=cli_globalsrc;
        char *in = (MORPHO_ISSTRING(module) ? MORPHO_GETCSTRING(module): NULL);
        
        if (in) src=cli_loadsource(in);
        
        if (src) {
            int n = 5;
            if (nlines) n = *nlines;
            
            int start = line - n, end = line + n;
            if (start<0) start = 0;
            
            cli_list(src, start, end);
            if (in) MORPHO_FREE(src);
        }
    }
}

/** Tell the CLI debugger to exit after this command */
void clidebugger_stop(clidebugger *debug) {
    debug->stop=true;
}
 
/* **********************************************************************
 * Info
 * ********************************************************************** */

/** Set info to display */
void clidebugger_setinfo(clidebugger *debug, char *info) {
    debug->info=info;
}

/** Clear info */
void clidebugger_clearinfo(clidebugger *debug) {
    debug->info=NULL;
}

/** Show info */
void clidebugger_showinfo(clidebugger *debug) {
    if (debug->info) cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1, debug->info);
}

/* **********************************************************************
 * Debugger lexer
 * ********************************************************************** */

enum {
    DEBUGGER_ASTERISK,
    DEBUGGER_DOT,
    DEBUGGER_EQ,
    DEBUGGER_COLON,
    DEBUGGER_QUOTE,
    
    DEBUGGER_INTEGER,
    
    DEBUGGER_ADDRESS,
    DEBUGGER_BREAK,
    DEBUGGER_CLEAR,
    DEBUGGER_CONTINUE,
    DEBUGGER_DISASSEMBLE,
    DEBUGGER_GARBAGECOLLECT,
    DEBUGGER_GLOBALS,
    DEBUGGER_G,
    DEBUGGER_HELP,
    DEBUGGER_INFO,
    DEBUGGER_LIST,
    DEBUGGER_PRINT,
    DEBUGGER_QUIT,
    DEBUGGER_REGISTERS,
    DEBUGGER_SET,
    DEBUGGER_STACK,
    DEBUGGER_STEP,
    DEBUGGER_TRACE,
    
    DEBUGGER_SYMBOL,
    DEBUGGER_STRING,
    
    DEBUGGER_EOF
};

/** @brief Lex strings
 *  @param[in]  l    the lexer
 *  @param[out] tok  token record to fill out
 *  @param[out] err  error struct to fill out on errors
 *  @returns true on success, false if an error occurs */
bool clidebugger_lexstring(lexer *l, token *tok, error *err) {
    while (lex_peek(l) != '"' && !lex_isatend(l)) {
        lex_advance(l);
    }
    
    if (lex_isatend(l)) {
        /* Unterminated string */
        morpho_writeerrorwithid(err, LEXER_UNTERMINATEDSTRING, NULL, l->line, l->posn);
        lex_recordtoken(l, TOKEN_NONE, tok);
        return false;
    }
    
    lex_advance(l); /* Closing quote */
    
    lex_recordtoken(l, DEBUGGER_STRING, tok);
    return true;
}

/** Debugger command tokens chosen to be largely compatible with GDB */
tokendefn debuggertokens[] = {
    { "*",              DEBUGGER_ASTERISK         , NULL },
    { ".",              DEBUGGER_DOT              , NULL },
    { "=",              DEBUGGER_EQ               , NULL },
    { ":",              DEBUGGER_COLON            , NULL },
    { "\"",             DEBUGGER_QUOTE            , clidebugger_lexstring },
    
    { "address",        DEBUGGER_ADDRESS          , NULL },
    
    { "break",          DEBUGGER_BREAK            , NULL },
    { "b",              DEBUGGER_BREAK            , NULL },
    
    { "backtrace",      DEBUGGER_TRACE            , NULL },
    { "bt",             DEBUGGER_TRACE            , NULL },
      
    { "clear",          DEBUGGER_CLEAR            , NULL },
    { "x",              DEBUGGER_CLEAR            , NULL },
    
    { "continue",       DEBUGGER_CONTINUE         , NULL },
    { "c",              DEBUGGER_CONTINUE         , NULL },
    
    { "disassemble",    DEBUGGER_DISASSEMBLE      , NULL },
    { "disassem",       DEBUGGER_DISASSEMBLE      , NULL },
    { "d",              DEBUGGER_DISASSEMBLE      , NULL },
    
    { "garbage",        DEBUGGER_GARBAGECOLLECT   , NULL },
    { "gc",             DEBUGGER_GARBAGECOLLECT   , NULL },
    
    { "globals",        DEBUGGER_GLOBALS          , NULL },
    { "global",         DEBUGGER_GLOBALS          , NULL },
    
    { "g",              DEBUGGER_G                , NULL },
    
    { "help",           DEBUGGER_HELP             , NULL },
    { "h",              DEBUGGER_HELP             , NULL },
    { "?",              DEBUGGER_HELP             , NULL },
    
    { "info",           DEBUGGER_INFO             , NULL },
    { "i",              DEBUGGER_INFO             , NULL },
    
    { "list",           DEBUGGER_LIST             , NULL },
    { "l",              DEBUGGER_LIST             , NULL },
    
    { "print",          DEBUGGER_PRINT            , NULL },
    { "p",              DEBUGGER_PRINT            , NULL },
    
    { "quit",           DEBUGGER_QUIT             , NULL },
    { "q",              DEBUGGER_QUIT             , NULL },
    
    { "registers",      DEBUGGER_REGISTERS        , NULL },
    { "register",       DEBUGGER_REGISTERS        , NULL },
    { "reg",            DEBUGGER_REGISTERS        , NULL },
    
    { "stack",          DEBUGGER_STACK            , NULL },
    
    { "step",           DEBUGGER_STEP             , NULL },
    { "s",              DEBUGGER_STEP             , NULL },
    
    { "set",            DEBUGGER_SET              , NULL },
    
    { "trace",          DEBUGGER_TRACE            , NULL },
    { "t",              DEBUGGER_TRACE            , NULL },
    
    { "",               TOKEN_NONE                , NULL }
};

/** Initialize the lexer */
void clidebugger_initializelexer(lexer *l, char *src) {
    lex_init(l, src, 0);
    lex_settokendefns(l, debuggertokens);
    lex_setnumbertype(l, DEBUGGER_INTEGER, TOKEN_NONE, TOKEN_NONE);
    lex_setsymboltype(l, DEBUGGER_SYMBOL);
    lex_seteof(l, DEBUGGER_EOF);
}

/* **********************************************************************
 * Debugger parser
 * ********************************************************************** */

/** Checks if a token type contains a keyword */
bool clidebugger_iskeyword(tokentype type) {
    return (type>DEBUGGER_INTEGER && type<DEBUGGER_SYMBOL);
}

/** Extracts a string from a token */
bool clidebugger_stringfromtoken(token *tok, value *out) {
    if (tok->length<2) return false;
    value str=object_stringfromcstring(tok->start+1, tok->length-2);
    bool success=!MORPHO_ISNIL(str);
    if (success) *out = str;
    return success;
}

/** Parses the next token as a symbol, returning it as a string */
bool clidebugger_parsesymbol(parser *p, clidebugger *debug, value *out) {
    bool success=false;
    
    if (parse_checktoken(p, DEBUGGER_SYMBOL) ||
        clidebugger_iskeyword(p->current.type)) {
        parse_advance(p);
        
        *out = parse_tokenasstring(p);
        success=true;
    }
    return success;
}

/** Breakpoints syntax:
    * integer x         = break at instruction given by x
    * integer x          = break at line x
    * symbol [ . symbol ] = break at function or method call */
bool clidebugger_parsebreakpoint(parser *p, clidebugger *debug, bool set) {
    value symbol=MORPHO_NIL, method=MORPHO_NIL;
    bool success=false;
    
    long instr=-1, line;
    if ((parse_checktokenadvance(p, DEBUGGER_ASTERISK) ||
         parse_checktokenadvance(p, DEBUGGER_ADDRESS)) &&
        parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
        parse_tokentointeger(p, &instr)) {
        success=debugger_breakatinstruction(debug->debug, set, (instructionindx) instr);
    } else if (parse_checktokenadvance(p, DEBUGGER_STRING) &&
               clidebugger_stringfromtoken(&p->previous, &symbol) &&
               parse_checkrequiredtoken(p, DEBUGGER_COLON, DBG_BRKFILE) &&
               parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
               parse_tokentointeger(p, &line)) {
        success=debugger_breakatline(debug->debug, set, symbol, (int) line);
    } else if (parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
               parse_tokentointeger(p, &line)) {
        success=debugger_breakatline(debug->debug, set, MORPHO_NIL, (int) line);
    } else if (clidebugger_parsesymbol(p, debug, &symbol)) {
        
        if (parse_checktokenadvance(p, DEBUGGER_DOT)) {
            if (clidebugger_parsesymbol(p, debug, &method)) {
                success=debugger_breakatfunction(debug->debug, set, symbol, method);
            } else parse_error(p, true, DBG_EXPCTMTHD);
        } else {
            success=debugger_breakatfunction(debug->debug, set, MORPHO_NIL, symbol);
        }
    } else clidebugger_setinfo(debug, DBG_BREAK_INFO);
    
    morpho_freeobject(symbol);
    morpho_freeobject(method);
    
    return success;
}

bool clidebugger_breakcommand(parser *p, void *out) {
    return clidebugger_parsebreakpoint(p, (clidebugger *) out, true);
}

bool clidebugger_clearcommand(parser *p, void *out) {
    return clidebugger_parsebreakpoint(p, (clidebugger *) out, false);
}

/** Continue command */
bool clidebugger_continuecommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_setsinglestep(debug->debug, false);
    clidebugger_stop(debug);
    return true;
}

/** Disassemble command */
bool clidebugger_disassemblecommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_disassemble(debugger_currentvm(debug->debug), debugger_currentvm(debug->debug)->current, &debug->debug->currentline);
    return true;
}

/** Run the garbage collector */
bool clidebugger_gccommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_garbagecollect(debug->debug);
    return true;
}

/** Display help */
bool clidebugger_helpcommand(parser *p, void *out) {
    char *info = DBG_HELP_INFO;
    
    if (parse_checktoken(p, DEBUGGER_BREAK)) {
        info = DBG_BREAK_INFO;
    } else if (parse_checktoken(p, DEBUGGER_INFO)) {
        info = DBG_INFO_INFO;
    } else if (parse_checktoken(p, DEBUGGER_SET)) {
        info = DBG_SET_INFO;
    }
    
    clidebugger_setinfo((clidebugger *) out, info);
    
    return true;
}

/** Info comand */
bool clidebugger_infocommand(parser *p, void *out) {
    long arg;
    clidebugger *debug = (clidebugger *) out;
    
    if (parse_checktokenadvance(p, DEBUGGER_ASTERISK) ||
        parse_checktokenadvance(p, DEBUGGER_ADDRESS)) { // Needs an integer argument
        if (parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
            parse_tokentointeger(p, &arg)) {
            debugger_showaddress(debug->debug, (registerindx) arg);
        }
    } else if (parse_checktokenadvance(p, DEBUGGER_BREAK)) {
        debugger_showbreakpoints(debug->debug);
    } else if (parse_checktokenadvance(p, DEBUGGER_GLOBALS) ||
               parse_checktokenadvance(p, DEBUGGER_G)) {
        if (parse_checktokenadvance(p, DEBUGGER_INTEGER)) {
            if (parse_tokentointeger(p, &arg)) {
                debugger_showglobal(debug->debug, (indx) arg);
            }
        } else debugger_showglobals(debug->debug);
    } else if (parse_checktokenadvance(p, DEBUGGER_REGISTERS)) {
        debugger_showregisters(debug->debug);
    } else if (parse_checktokenadvance(p, DEBUGGER_STACK) ||
               parse_checktokenadvance(p, DEBUGGER_STEP)) {
        debugger_showstack(debug->debug);
    } else {
        if (!parse_checktoken(p, DEBUGGER_EOF)) parse_error(p, true, DBG_INFO);
        clidebugger_setinfo(debug, DBG_INFO_INFO);
        return false;
    }
    
    return true;
}

/** List the program */
bool clidebugger_listcommand(parser *p, void *out) {
    long nlinesl;
    int nlines=-1;
    if (parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
        parse_tokentointeger(p, &nlinesl)) {
        nlines=(int) nlinesl;
    }
    
    clidebugger_list((clidebugger *) out, (nlines > 0 ? &nlines : NULL));
    return true;
}

/** Prints the contents of a symbol */
bool clidebugger_printcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    value symbol = MORPHO_NIL, prop = MORPHO_NIL;
    if (clidebugger_parsesymbol(p, debug, &symbol)) {
        if (parse_checktokenadvance(p, DEBUGGER_DOT) &&
            clidebugger_parsesymbol(p, debug, &prop)) {
            debugger_showproperty(debug->debug, symbol, prop);
        } else {
            debugger_showsymbol(debug->debug, symbol);
        }
    } else /*if (parse_checktoken(p, DEBUGGER_EOF))*/ {
        debugger_showsymbols(debug->debug);
    }
    morpho_freeobject(symbol);
    morpho_freeobject(prop);
    
    return true;
}

/** Quit the debugger */
bool clidebugger_quitcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_quit(debug->debug);
    clidebugger_stop(debug);
    return true;
}

/** Sets a register, variable or property to a specified (leaf) value */
bool clidebugger_setcommand(parser *p, void *out) {
    bool success=false;
    clidebugger *debug = (clidebugger *) out;
    long reg;
    value symbol = MORPHO_NIL, prop = MORPHO_NIL, val = MORPHO_NIL;
    enum { SET_NONE, SET_REGISTER, SET_VAR, SET_PROPERTY } mode = SET_NONE;
    
    if (parse_checktokenadvance(p, DEBUGGER_REGISTERS) &&
        parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
        parse_tokentointeger(p, &reg)) {
        mode=SET_REGISTER;
    } else if (clidebugger_parsesymbol(p, debug, &symbol)) {
        mode=SET_VAR;
        if (parse_checktokenadvance(p, DEBUGGER_DOT) &&
            clidebugger_parsesymbol(p, debug, &prop)) {
            mode=SET_PROPERTY;
        }
    }
    
    if (parse_checktokenadvance(p, DEBUGGER_EQ) &&
        parse_value(p->lex->start, &val)) {
        switch (mode) {
            case SET_REGISTER:
                success=debugger_setregister(debug->debug, (indx) reg, val);
                break;
            case SET_VAR:
                success=debugger_setsymbol(debug->debug, symbol, val);
                break;
            case SET_PROPERTY:
                success=debugger_setproperty(debug->debug, symbol, prop, val);
                break;
            case SET_NONE:
                break;
        }
    } else {
        parse_error(p, true, DBG_PRS);
    }
     
    morpho_freeobject(symbol);
    morpho_freeobject(prop);
    
    if (!success) clidebugger_setinfo(debug, DBG_SET_INFO);
    
    return success;
}

/** Single step */
bool clidebugger_stepcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_setsinglestep(debug->debug, true);
    clidebugger_stop(debug);
    return true;
}

/** Display stack trace */
bool clidebugger_tracecommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    morpho_stacktrace(debugger_currentvm(debug->debug));
    return true;
}

/** Parses a debugger command using the parse table */
bool clidebugger_parsecommand(parser *p, void *out) {
    if (!parse_incrementrecursiondepth(p)) return false; // Increment and check
    
    bool success=parse_precedence(p, PREC_ASSIGN, out);
    
    parse_decrementrecursiondepth(p);
    return success;
}

/* -------------------------------------------------------
 * Debugger parse table associates tokens to parselets
 * ------------------------------------------------------- */

parserule clidebugger_rules[] = {
    PARSERULE_PREFIX(DEBUGGER_BREAK, clidebugger_breakcommand),
    PARSERULE_PREFIX(DEBUGGER_CLEAR, clidebugger_clearcommand),
    PARSERULE_PREFIX(DEBUGGER_CONTINUE, clidebugger_continuecommand),
    PARSERULE_PREFIX(DEBUGGER_DISASSEMBLE, clidebugger_disassemblecommand),
    PARSERULE_PREFIX(DEBUGGER_GARBAGECOLLECT, clidebugger_gccommand),
    PARSERULE_PREFIX(DEBUGGER_G, clidebugger_gccommand),
    PARSERULE_PREFIX(DEBUGGER_HELP, clidebugger_helpcommand),
    PARSERULE_PREFIX(DEBUGGER_INFO, clidebugger_infocommand),
    PARSERULE_PREFIX(DEBUGGER_LIST, clidebugger_listcommand),
    PARSERULE_PREFIX(DEBUGGER_PRINT, clidebugger_printcommand),
    PARSERULE_PREFIX(DEBUGGER_QUIT, clidebugger_quitcommand),
    PARSERULE_PREFIX(DEBUGGER_SET, clidebugger_setcommand),
    PARSERULE_PREFIX(DEBUGGER_STEP, clidebugger_stepcommand),
    PARSERULE_PREFIX(DEBUGGER_TRACE, clidebugger_tracecommand),
    PARSERULE_UNUSED(TOKEN_NONE)
};

/* -------------------------------------------------------
 * Initialize the debugger parser
 * ------------------------------------------------------- */

/** Initializes a parser to parse debugger commands */
void clidebugger_initializeparser(parser *p, lexer *l, error *err, void *out) {
    parse_init(p, l, err, out);
    parse_setbaseparsefn(p, clidebugger_parsecommand);
    parse_setparsetable(p, clidebugger_rules);
    parse_setskipnewline(p, false, TOKEN_NONE);
}

/** Parses debugging commands */
bool clidebugger_parse(clidebugger *debug, char *in) {
    lexer l;
    clidebugger_initializelexer(&l, in);

    parser p;
    clidebugger_initializeparser(&p, &l, debug->err, debug);
    
    bool success=parse(&p);
    
    if (!success && morpho_matcherror(p.err, PARSE_EXPECTEXPRESSION)) {
        error_clear(p.err); // Clear the error and replace with invalid command
        parse_error(&p, true, DBG_INVLD);
        clidebugger_setinfo(debug, DBG_HELP_INFO);
    }
    
    parse_clear(&p);
    lex_clear(&l);
    
    return success;
}

/* **********************************************************************
 * Debugger REPL
 * ********************************************************************** */

void clidebugger_enter(vm *v) {
    error err;
    error_init(&err);
    
    lineditor edit;
    linedit_init(&edit);
    linedit_setprompt(&edit, DEBUGGER_PROMPT);
    
    clidebugger debug;
    clidebugger_init(&debug, v, &edit, &err);
    clidebugger_banner(&debug);
    
    while (!debug.stop) {
        clidebugger_clearinfo(&debug);
        char *input = linedit(&edit);
        if (!input) break;
        
        if (!clidebugger_parse(&debug, input) ||
            morpho_checkerror(&err)) {
            clidebugger_reporterror(&debug);
            error_clear(&err);
        }
        
        clidebugger_showinfo(&debug);
    }
    
    clidebugger_resumebanner(&debug);
    
    linedit_clear(&edit);
    error_clear(&err);
}

void clidebugger_initialize(void) {
    morpho_defineerror(DBG_PRS, ERROR_PARSE, DBG_PRS_MSG);
    morpho_defineerror(DBG_INFO, ERROR_PARSE, DBG_INFO_MSG);
    morpho_defineerror(DBG_INVLD, ERROR_PARSE, DBG_INVLD_MSG);
    morpho_defineerror(DBG_EXPCTMTHD, ERROR_PARSE, DBG_EXPCTMTHD_MSG);
    morpho_defineerror(DBG_BRKFILE, ERROR_PARSE, DBG_BRKFILE_MSG);
}
