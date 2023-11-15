/** @file debugger.c
 *  @author T J Atherton
 *
 *  @brief Command line debugger
*/

#include "morpho.h"
#include "debugger.h"

/** Display the morpho banner */
void debugger_banner(vm *v, lineditor *edit) {
    cli_displaywithstyle(edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Morpho debugger---\n");
    cli_displaywithstyle(edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "Type '?' or 'h' for help.\n");
    
 /* printf("%s ", (debug->singlestep ? "Single stepping" : "Breakpoint"));
  debugger_printlocation(v, debug, debug->iindx);
  printf("\n");
    */
}

/** Display the resume text */
void debugger_resumebanner(lineditor *edit) {
    cli_displaywithstyle(edit, DEBUGGER_COLOR, CLI_NOEMPHASIS, 1, "---Resuming----------\n");
}

void debugger_enter(vm *v) {
    lineditor edit;
    
    linedit_init(&edit);
    linedit_setprompt(&edit, DEBUGGER_PROMPT);
    
    debugger_banner(v, &edit);
    
    linedit(&edit);
    
    debugger_resumebanner(&edit);
    
    linedit_clear(&edit);
}

