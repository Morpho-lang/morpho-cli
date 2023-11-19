/** @file debugger.c
 *  @author T J Atherton
 *
 *  @brief Command line debugger
*/

#include "compile.h"
#include "vm.h"
#include "parse.h"
#include "debug.h"

#include "debugger.h"
#include "gc.h"

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
    bool stop;
} clidebugger;

void clidebugger_init(clidebugger *debug, vm *v, lineditor *edit, error *err) {
    debug->debug=vm_getdebugger(v);
    debug->edit=edit;
    debug->err=err;
    debug->stop=false;
}

/* **********************************************************************
 * Debugger functions
 * ********************************************************************** */

/** Display the morpho banner */
void clidebugger_banner(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Morpho debugger---\n");
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "Type '?' or 'h' for help.\n");
    
    morpho_printf(debugger_currentvm(debug->debug), "%s ", (debug->debug->singlestep ? "Single stepping" : "Breakpoint"));
    debugger_showlocation(debug->debug, debug->debug->iindx);
    
    morpho_printf(debugger_currentvm(debug->debug), "\n");
}

/** Display the resume text */
void clidebugger_resumebanner(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Resuming----------\n");
}

/** Source listing */
void clidebugger_list(clidebugger *debug) {
    int line=0;
    value module=MORPHO_NIL;
    vm *v = debugger_currentvm(debug->debug);
    
    if (debug_infofromindx(v->current, vm_previnstruction(v), &module, &line, NULL, NULL, NULL)) {
        cli_list((MORPHO_ISSTRING(module) ? MORPHO_GETCSTRING(module): NULL), line-5, line+5);
    }

}

/** Tell the CLI debugger to exit after this command */
void clidebugger_stop(clidebugger *debug) {
    debug->stop=true;
}
 
/* **********************************************************************
 * Help
 * ********************************************************************** */

/** Debugger help */
void clidebugger_help(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1,
        "Available commands:\n"
        "  [b]reakpoint, [c]ontinue, [d]isassemble, [g]arbage collect,\n"
        "  [?]/[h]elp, [i]nfo, [l]ist, [p]rint, [q]uit, [s]tep, \n"
        "  [t]race, [x]clear\n");
}

/** Info help */
void clidebugger_infohelp(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1,
        "Valid info commands: \n"
        "  info address n: Displays the address of register n.\n"
        "  info break    : Displays all breakpoints.\n"
        "  info globals  : Displays the contents of all globals.\n"
        "  info global n : Displays the contents of global n.\n"
        "  info registers: Displays the contents of all registers.\n"
        "  info stack    : Displays the stack.\n");
}

/** Break command help */
void clidebugger_breakhelp(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1,
        "Valid break commands (same syntax for clear): \n"
        "  break * n     : Break at instruction n.\n"
        "  break n       : Break at line n\n"
        "  break <symbol>: Break at a function or method.\n");
}

/** Set command help */
void clidebugger_sethelp(clidebugger *debug) {
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1,
        "Valid set commands: \n"
        "  set register n = X : Sets register n to X.\n"
        "  set <symbol> = X   : Sets symbol to X.\n");
}

/* **********************************************************************
 * Debugger lexer
 * ********************************************************************** */

enum {
    DEBUGGER_ASTERISK,
    DEBUGGER_DOT,
    DEBUGGER_EQ,
    
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
    
    DEBUGGER_EOF
};

/** Debugger command tokens chosen to be largely compatible with GDB */
tokendefn debuggertokens[] = {
    { "*",              DEBUGGER_ASTERISK         , NULL },
    { ".",              DEBUGGER_DOT              , NULL },
    { "=",              DEBUGGER_EQ               , NULL },
    
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

/** Parses a symbol */
bool clidebugger_parsesymbol(parser *p, clidebugger *debug) {
    return parse_checktokenadvance(p, DEBUGGER_SYMBOL);
}

/** Breakpoints syntax:
    * integer x         = break at instruction given by x
    * integer x          = break at line x
    * symbol [ . symbol ] = break at function or method call */
bool clidebugger_parsebreakpoint(parser *p, clidebugger *debug, bool set) {
    long instr;
    if (parse_checktokenadvance(p, DEBUGGER_ASTERISK) &&
        parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
        parse_tokentointeger(p, &instr)) {
        printf("break *%i\n", (int) instr);
    } else if (parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
               parse_tokentointeger(p, &instr)) {
        printf("break %i\n", (int) instr);
    } else if (clidebugger_parsesymbol(p, debug)) {
        printf("break <<symbol>>\n");
    } else {
        clidebugger_breakhelp(debug);
    }
    return true;
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
    debug_disassemble(debugger_currentvm(debug->debug)->current, debug->debug->currentline);
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
    clidebugger_help((clidebugger *) out);
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
            printf("Info asterisk %i\n", (int) arg);
            //debugger_showaddress(debug->debug, (registerindx) arg);
            return true;
        }
    } else if (parse_checktokenadvance(p, DEBUGGER_BREAK)) {
        //debugger_showbreakpoints(v, debug);
        return true;
    } else if (parse_checktokenadvance(p, DEBUGGER_GLOBALS) ||
               parse_checktokenadvance(p, DEBUGGER_G)) {
        if (parse_checktokenadvance(p, DEBUGGER_INTEGER)) {
            if (parse_tokentointeger(p, &arg)) {
                printf("Info global %i\n", (int) arg);
                return true;
            }
        } else {
            printf("Info globals\n");
        }
        return true;
    } else if (parse_checktokenadvance(p, DEBUGGER_REGISTERS)) {
        //debug_showregisters(debug->v);
        return true;
    } else if (parse_checktokenadvance(p, DEBUGGER_STACK) ||
               parse_checktokenadvance(p, DEBUGGER_STEP)) {
        //debug_showstack(debug->v);
        return true;
    }

    clidebugger_infohelp(debug);
    
    return false;
}

/** List the program */
bool clidebugger_listcommand(parser *p, void *out) {
    //clidebugger_list((clidebugger *) out);
    return true;
}

bool clidebugger_printcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    if (clidebugger_parsesymbol(p, debug)) {
        
    }
    return true;
}

/** Quit the debugger */
bool clidebugger_quitcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    debugger_quit(debug->debug);
    clidebugger_stop(debug);
    return true;
}

bool clidebugger_setcommand(parser *p, void *out) {
    clidebugger *debug = (clidebugger *) out;
    long reg;
    
    if (parse_checktokenadvance(p, DEBUGGER_REGISTERS) &&
        parse_checktokenadvance(p, DEBUGGER_INTEGER) &&
        parse_tokentointeger(p, &reg)) {
        printf("set register %i \n", (int) reg);
    } else if (parse_checktokenadvance(p, DEBUGGER_SYMBOL)) {
        printf("set <symbol>\n");
    }
    
    if (!parse_checktokenadvance(p, DEBUGGER_EQ)) goto clidebugger_setcommanderr;
        
    clidebugger_parsesymbol(p, debug);
    
    return true;
    
clidebugger_setcommanderr:
    clidebugger_sethelp(debug);
    
    return false;
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
}

/** Parses debugging commands */
bool clidebugger_parse(clidebugger *debug, char *in) {
    lexer l;
    clidebugger_initializelexer(&l, in);

    parser p;
    clidebugger_initializeparser(&p, &l, debug->err, debug);
    
    bool success=parse(&p);
    
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
        char *input = linedit(&edit);
        if (!input) break;
        
        clidebugger_parse(&debug, input);
    }
    
    clidebugger_resumebanner(&debug);
    
    linedit_clear(&edit);
    error_clear(&err);
}
