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

#include "inline.h"
#include "hlp.h"

#define CLI_DEFAULTCOLOR -1
#define CLI_ERRORCOLOR  INLINE_RED
#define CLI_WARNINGCOLOR  INLINE_YELLOW
#define CLI_HINTCOLOR  8 // This should be an inline color macro 'INLINE_BRIGHTBLACK' but that is yet to exist atm

#define CLI_NOEMPHASIS  -1
#define CLI_BOLD        0
#define CLI_UNDERLINE   1
#define CLI_ITALIC      2

#define CLI_PROMPT "> "
#define CLI_CONTINUATIONPROMPT "~ "
#define CLI_QUIT "quit"
#define CLI_HELP "help"
#define CLI_SHORT_HELP "?"

#define CLI_RUN                 (1<<0)
#define CLI_DISASSEMBLE         (1<<1)
#define CLI_DISASSEMBLESHOWSRC  (1<<2)
#define CLI_DEBUG               (1<<3)
#define CLI_OPTIMIZE            (1<<4)
#define CLI_PROFILE             (1<<5)
#define CLI_INTERACTIVE         (1<<6)
#define CLI_NOCOLOR             (1<<7)
#define CLI_NOHINTS             (1<<8)

typedef unsigned int clioptions;

extern char *cli_globalsrc;
void cli_setexitcode(int code);
int cli_exitcode(void);
void cli_applyoptions(clioptions opt);
bool cli_usecolor(void);

void cli_displaywithstyle(int col, int emph, int n, ...);
void cli_emitemphasis(int emph);
void cli_reporterror(error *err, vm *v);

void cli_run(const char *in, clioptions opt, const char *preamble);
void cli_runstring(const char *src, clioptions opt, const char *preamble);
void cli(clioptions opt, const char *preamble);

char *cli_loadsource(const char *in);
char *cli_loadstdin(void);
void cli_disassemblewithsrc(program *p, char *src, clioptions opt);
void cli_list(const char *in, int start, int end, clioptions opt);
void cli_helpquery(const char *query, clioptions opt);

void clidebugger_enter(vm *v);
void clidebugger_initialize(void);

#endif /* cli_h */


