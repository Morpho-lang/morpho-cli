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

void clidebugger_enter(vm *v);

#endif /* debugger_h */
