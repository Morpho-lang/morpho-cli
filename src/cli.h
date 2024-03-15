/** @file cli.h
 *  @author T J Atherton
 *
 *  @brief Command line interface
*/

#ifndef cli_h
#define cli_h

#include <morpho.h>
#include <varray.h>
#include <common.h>

#include "linedit.h"
#include "help.h"

#include "debugger.h"

#define CLI_DEFAULTCOLOR LINEDIT_DEFAULTCOLOR
#define CLI_ERRORCOLOR  LINEDIT_RED
#define CLI_WARNINGCOLOR  LINEDIT_YELLOW
#define CLI_NOEMPHASIS  LINEDIT_NONE

#define CLI_PROMPT ">"
#define CLI_CONTINUATIONPROMPT "~"
#define CLI_QUIT "quit"
#define CLI_HELP "help"
#define CLI_SHORT_HELP "?"

#define CLI_RUN                 (1<<0)
#define CLI_DISASSEMBLE         (1<<1)
#define CLI_DISASSEMBLESHOWSRC  (1<<2)
#define CLI_DEBUG               (1<<3)
#define CLI_OPTIMIZE            (1<<4)
#define CLI_PROFILE             (1<<5)

typedef unsigned int clioptions;

extern char *cli_globalsrc;

void cli_displaywithstyle(lineditor *edit, linedit_color col, linedit_emphasis emph, int n, ...);
void cli_reporterror(error *err, vm *v);


void cli_run(const char *in, clioptions opt);
void cli(clioptions opt);

char *cli_loadsource(const char *in);
void cli_disassemblewithsrc(program *p, char *src);
void cli_list(const char *in, int start, int end);

#endif /* cli_h */
