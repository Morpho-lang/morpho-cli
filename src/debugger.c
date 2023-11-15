/** @file debugger.c
 *  @author T J Atherton
 *
 *  @brief Command line debugger
*/

#include "morpho.h"
#include "debugger.h"
#include "parse.h"

/** @brief Debugger front end
 *
 * @details The debugger front end is implemented using the new lexer and parser:
 * - we define all debugger commands and shortcuts as tokens
 * - debugger commands are implemented via parselets */

/* **********************************************************************
 * Debugger front end structure
 * ********************************************************************** */

typedef struct {
    vm *v;  /** Current VM in use */
    lineditor *edit; /** lineeditor for output */
    error *err; /** Error structure to fill out  */
    bool stop;
} debugger;

void debugger_init(debugger *debug, vm *v, lineditor *edit, error *err) {
    debug->v=v;
    debug->edit=edit;
    debug->err=err;
    debug->stop=false;
}

/* **********************************************************************
 * Debugger functions
 * ********************************************************************** */

/** Display the morpho banner */
void debugger_banner(debugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Morpho debugger---\n");
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "Type '?' or 'h' for help.\n");
    
 /* printf("%s ", (debug->singlestep ? "Single stepping" : "Breakpoint"));
  debugger_printlocation(v, debug, debug->iindx);
  printf("\n");
    */
}

/** Display the resume text */
void debugger_resumebanner(debugger *debug) {
    cli_displaywithstyle(debug->edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Resuming----------\n");
}

/** Debugger help */
void debugger_help(debugger *debug) {
    cli_displaywithstyle(debug->edit, CLI_DEFAULTCOLOR, CLI_NOEMPHASIS, 1,
        "Available commands:\n"
        "  [b]reakpoint, [c]ontinue, [d]isassemble, [g]arbage collect,\n"
        "  [?]/[h]elp, [i]nfo, [l]ist, [p]rint, [q]uit, [s]tep, \n"
        "  [t]race, [x]clear\n");
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
void debugger_initializelexer(lexer *l, char *src) {
    lex_init(l, src, 0);
    lex_settokendefns(l, debuggertokens);
    //lex_setprefn(l, json_lexpreprocess);
    lex_seteof(l, DEBUGGER_EOF);
}

/* **********************************************************************
 * Debugger parser
 * ********************************************************************** */

bool debugger_breakcommand(parser *p, void *out) {
}

bool debugger_clearcommand(parser *p, void *out) {
}

bool debugger_continuecommand(parser *p, void *out) {
}

bool debugger_disassemblecommand(parser *p, void *out) {
}

bool debugger_gccommand(parser *p, void *out) {
    //vm_collectgarbage(((debugger *) out)->v);
}

/** Display help */
bool debugger_helpcommand(parser *p, void *out) {
    debugger_help((debugger *) out);
}

bool debugger_infocommand(parser *p, void *out) {
}

bool debugger_listcommand(parser *p, void *out) {
}

bool debugger_printcommand(parser *p, void *out) {
}

/** Quit the debugger */
bool debugger_quitcommand(parser *p, void *out) {
    debugger *debug = (debugger *) out;
    debug->stop=true;
}

bool debugger_setcommand(parser *p, void *out) {
}

bool debugger_stepcommand(parser *p, void *out) {
}

/** Display stack trace */
bool debugger_tracecommand(parser *p, void *out) {
    morpho_stacktrace(((debugger *) out)->v);
}

/** Parses a debugger command using the parse table */
bool debugger_parsecommand(parser *p, void *out) {
    if (!parse_incrementrecursiondepth(p)) return false; // Increment and check
    
    bool success=parse_precedence(p, PREC_ASSIGN, out);
    
    parse_decrementrecursiondepth(p);
    return success;
}

/* -------------------------------------------------------
 * Debugger parse table associates tokens to parselets
 * ------------------------------------------------------- */

parserule debugger_rules[] = {
    PARSERULE_PREFIX(DEBUGGER_BREAK, debugger_breakcommand),
    PARSERULE_PREFIX(DEBUGGER_CLEAR, debugger_clearcommand),
    PARSERULE_PREFIX(DEBUGGER_CONTINUE, debugger_continuecommand),
    PARSERULE_PREFIX(DEBUGGER_DISASSEMBLE, debugger_disassemblecommand),
    PARSERULE_PREFIX(DEBUGGER_GARBAGECOLLECT, debugger_gccommand),
    PARSERULE_PREFIX(DEBUGGER_HELP, debugger_helpcommand),
    PARSERULE_PREFIX(DEBUGGER_INFO, debugger_infocommand),
    PARSERULE_PREFIX(DEBUGGER_LIST, debugger_listcommand),
    PARSERULE_PREFIX(DEBUGGER_PRINT, debugger_printcommand),
    PARSERULE_PREFIX(DEBUGGER_QUIT, debugger_quitcommand),
    PARSERULE_PREFIX(DEBUGGER_SET, debugger_setcommand),
    PARSERULE_PREFIX(DEBUGGER_STEP, debugger_stepcommand),
    PARSERULE_PREFIX(DEBUGGER_TRACE, debugger_tracecommand),
    PARSERULE_UNUSED(TOKEN_NONE)
};

/* -------------------------------------------------------
 * Initialize the debugger parser
 * ------------------------------------------------------- */

/** Initializes a parser to parse debugger commands */
void debugger_initializeparser(parser *p, lexer *l, error *err, void *out) {
    parse_init(p, l, err, out);
    parse_setbaseparsefn(p, debugger_parsecommand);
    parse_setparsetable(p, debugger_rules);
}

/** Parses debugging commands */
bool debugger_parse(debugger *debug, char *in) {
    lexer l;
    debugger_initializelexer(&l, in);

    parser p;
    debugger_initializeparser(&p, &l, debug->err, debug);
    
    bool success=parse(&p);
    
    parse_clear(&p);
    lex_clear(&l);
    
    return success;
}

/* **********************************************************************
 * Debugger REPL
 * ********************************************************************** */

void debugger_enter(vm *v) {
    error err;
    error_init(&err);
    
    lineditor edit;
    linedit_init(&edit);
    linedit_setprompt(&edit, DEBUGGER_PROMPT);
    
    debugger debug;
    debugger_init(&debug, v, &edit, &err);
    debugger_banner(&debug);
    
    while (!debug.stop) {
        char *input = linedit(&edit);
        if (!input) break;
        
        debugger_parse(&debug, input);
    }
    
    debugger_resumebanner(&debug);
    
    linedit_clear(&edit);
    error_clear(&err);
}
