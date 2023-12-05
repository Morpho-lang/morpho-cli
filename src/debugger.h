/** @file debugger.h
 *  @author T J Atherton
 *
 *  @brief Command line debugger
*/

#ifndef debugger_h
#define debugger_h

#include "cli.h"

#define DEBUGGER_PROMPT "@>"

#define DEBUGGER_COLOR LINEDIT_GREEN
#define DEBUGGER_ERROR_COLOR LINEDIT_RED

#define DBG_PRS                         "DbgPrs"
#define DBG_PRS_MSG                     "Couldn't parse command."

#define DBG_EXPCTMTHD                   "DbgExpctMthd"
#define DBG_EXPCTMTHD_MSG               "Expected method label."

#define DBG_INVLD                       "DbgInvld"
#define DBG_INVLD_MSG                   "Invalid debugger command."

#define DBG_INFO                        "DbgInfo"
#define DBG_INFO_MSG                    "Invalid info command."

#define DBG_HELP_INFO      "Available commands:\n" \
    "  [b]reakpoint, [c]ontinue, [d]isassemble, [g]arbage collect,\n" \
    "  [?]/[h]elp, [i]nfo, [l]ist, [p]rint, [q]uit, [s]tep, \n" \
    "  [t]race, [x]clear\n"

#define DBG_INFO_INFO      "Possible info commands: \n" \
    "  info address n: Displays the address of register n.\n" \
    "  info break    : Displays all breakpoints.\n" \
    "  info globals  : Displays the contents of all globals.\n" \
    "  info global n : Displays the contents of global n.\n" \
    "  info registers: Displays the contents of all registers.\n" \
    "  info stack    : Displays the stack.\n"

#define DBG_BREAK_INFO     "Possible break commands (same syntax for clear): \n" \
    "  break * n     : Break at instruction n.\n" \
    "  break n       : Break at line n\n" \
    "  break <symbol>: Break at a function or method.\n"

#define DBG_SET_INFO     "Possible set commands: \n" \
    "  set register n = X : Sets register n to X.\n" \
    "  set <symbol> = X   : Sets symbol to X.\n"

void clidebugger_enter(vm *v);

void clidebugger_initialize(void);

#endif /* debugger_h */
