/** @file main.c
 *  @author T J Atherton
 *
 *  @brief Main entry point and process options
 */

#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include <morpho.h>

#include "cli.h"
#include "debugger.h"
#include "inline.h"

/** Context passed to option handlers that need argc/argv (e.g. eval). */
typedef struct {
    int argc;
    const char **argv;
    int *idx;
} opt_ctx;

/** Option handler. arg is NULL for options that take no argument.
 *  @returns true to continue (run file/repl), false to exit without running. */
typedef bool (*optionfn)(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx);

static bool opt_version(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)flags; (void)ctx;
    version v;
    morpho_version(&v);
    char buf[VERSION_MAXSTRINGLENGTH];
    version_tostring(&v, VERSION_MAXSTRINGLENGTH, buf);
    printf("Morpho v%s\n", buf);
    return false;
}

static bool opt_help(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg;
    /* Optional query: morpho6 --help [topic] looks up built-in language help. */
    if (ctx && ctx->idx && *ctx->idx + 1 < ctx->argc) {
        const char *next = ctx->argv[*ctx->idx + 1];
        if (next && next[0] != '-') {
            (*ctx->idx)++;
            cli_helpquery(next, *flags);
            return false;
        }
    }
    printf("Usage: morpho6 [options] [file] [options passed to program]\n");
    printf("\nOptions:\n");
    printf("  -h, --help [query]      Show this help, or look up a language help topic\n");
    printf("  -v, --version           Show version information\n");
    printf("  -c, --check             Check syntax without executing\n");
    printf("  -e, --eval <code>       Execute code string (before file/REPL if combined)\n");
    printf("  -i, --interactive       Enter REPL after running file\n");
    printf("  -l, --list <file>       List file with syntax highlighting\n");
    printf("  -d, --disassemble       Show disassembly\n");
    printf("  -D                      Disassemble only (no execution)\n");
    printf("  -dl                     Disassemble with source listing\n");
    printf("  -debug, --debug         Enable debugger\n");
    printf("  -O, --optimize          Enable optimizations\n");
    printf("  -profile, --profile     Enable profiling\n");
    printf("  --no-color              Disable syntax highlighting\n");
    printf("  --no-hints              Disable hint output\n");
    printf("  -w, --workers <n>       Set number of worker threads\n");
    printf("\nIf no file is specified, morpho enters interactive REPL mode.\n");
    printf("If stdin is piped or redirected, morpho reads and executes from stdin.\n");
    printf("Any options after the file name are passed to the morpho program.\n");
    return false;
}

static bool opt_disassembleonly(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags &= ~CLI_RUN;
    *flags |= CLI_DISASSEMBLE;
    return true;
}

static bool opt_disassemblelist(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_DISASSEMBLE | CLI_DISASSEMBLESHOWSRC;
    return true;
}

static bool opt_disassemble(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_DISASSEMBLE;
    return true;
}

static bool opt_debug(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_DEBUG;
    return true;
}

static bool opt_optimize(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_OPTIMIZE;
    return true;
}

static bool opt_profile(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_PROFILE;
    return true;
}

static bool opt_workers(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)flags; (void)ctx;
    if (!arg || !isdigit((unsigned char)*arg)) {
        fprintf(stderr, "morpho: -w/--workers requires a number.\n");
        cli_setexitcode(EXIT_FAILURE);
        return false;
    }
    int n = atoi(arg);
    if (n < 0) n = 0;
    morpho_setthreadnumber(n);
    return true;
}

static bool opt_nocolor(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_NOCOLOR;
    return true;
}

static bool opt_nohints(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_NOHINTS;
    return true;
}

static bool opt_check(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags &= ~CLI_RUN; /* Clear RUN flag - compile only, don't execute */
    return true;
}

static bool opt_interactive(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)arg; (void)ctx;
    *flags |= CLI_INTERACTIVE; // Enter REPL after running file 
    return true;
}

static bool opt_list(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)ctx;
    if (!arg) {
        cli_setexitcode(EXIT_FAILURE);
        return false;
    }
    
    char *src = cli_loadsource(arg);
    if (src) {
        cli_list(src, 1, INT_MAX, *flags);
        MORPHO_FREE(src);
    } else {
        fprintf(stderr, "morpho: Could not open file '%s'\n", arg);
        cli_setexitcode(EXIT_FAILURE);
    }
    return false; // Don't run program after listing 
}

static varray_char evalcode;

static bool opt_eval(const char *opt, const char *arg, clioptions *flags, opt_ctx *ctx) {
    (void)opt; (void)flags; (void)ctx;
    if (!arg) return false;
    /* Accumulate -e snippets; they are compiled before the file/REPL. */
    if (evalcode.count > 0) {
        evalcode.count--; /* drop trailing NUL */
        varray_charwrite(&evalcode, '\n');
    }
    varray_charadd(&evalcode, (char *) arg, (int) strlen(arg));
    varray_charwrite(&evalcode, '\0');
    return true;
}

typedef struct {
    const char *s, *l;
    bool takes_arg;
    optionfn fn;
} option_t;

static const option_t opt_table[] = {
    { "-D",       NULL,            false, opt_disassembleonly },
    { "-dl",      NULL,            false, opt_disassemblelist },
    { "-debug",   "--debug",       false, opt_debug },
    { "-d",       "--disassemble", false, opt_disassemble },
    { "-c",       "--check",       false, opt_check },
    { "-e",       "--eval",        true,  opt_eval },
    { "-h",       "--help",        false, opt_help },
    { "-i",       "--interactive", false, opt_interactive },
    { "-l",       "--list",        true,  opt_list },
    { "-O",       "--optimize",    false, opt_optimize },
    { "-profile", "--profile",     false, opt_profile },
    { NULL,       "--no-color",    false, opt_nocolor },
    { NULL,       "--no-hints",    false, opt_nohints },
    { "-v",       "--version",     false, opt_version },
    { "-w",       "--workers",     true,  opt_workers },
    { NULL,       NULL,            false, NULL },
};

/** Match argv token to an option name. Exact match, or for options that take an
 *  argument: name=value, or (short options only) -w4 with the value attached. */
static bool option_matches(const char *arg, const char *name, bool takes_arg, const char **attached) {
    if (!name) return false;
    size_t n = strlen(name);
    if (strncmp(arg, name, n) != 0) return false;
    if (arg[n] == '\0') {
        *attached = NULL;
        return true;
    }
    if (!takes_arg) return false;
    if (arg[n] == '=') {
        *attached = arg + n + 1;
        return true;
    }
    /* Short option with attached value, e.g. -w4 */
    if (name[0] == '-' && name[1] != '-' && isdigit((unsigned char)arg[n])) {
        *attached = arg + n;
        return true;
    }
    return false;
}

/** Parse one option at argv[*idx]. If option takes an argument, consumes *idx+1 and advances *idx. */
static bool parse_option(int argc, const char *argv[], int *idx, clioptions *flags) {
    const char *arg = argv[*idx], *opt_arg = NULL;
    for (int j = 0; opt_table[j].s || opt_table[j].l; j++) {
        const char *attached = NULL;
        bool match = option_matches(arg, opt_table[j].s, opt_table[j].takes_arg, &attached) ||
                     option_matches(arg, opt_table[j].l, opt_table[j].takes_arg, &attached);
        if (!match) continue;

        if (opt_table[j].takes_arg) {
            if (attached) {
                opt_arg = attached;
            } else if (*idx + 1 >= argc) {
                fprintf(stderr, "morpho: %s requires an argument.\n", arg);
                cli_setexitcode(EXIT_FAILURE);
                return false;
            } else {
                opt_arg = argv[*idx + 1];
                (*idx)++;
            }
        }
        opt_ctx ctx = { argc, argv, idx };
        return opt_table[j].fn(arg, opt_arg, flags, &ctx);
    }
    fprintf(stderr, "morpho: Unknown option %s.\n", arg);
    cli_setexitcode(EXIT_FAILURE);
    return false;
}

int main(int argc, const char *argv[]) {
    clioptions opt = CLI_RUN;
    const char *file = NULL;
    int i = 1;
    bool run = true;

    morpho_initialize();
    cli_setexitcode(EXIT_SUCCESS);
    varray_charinit(&evalcode);

    for (; i < argc && !file; i++) {
        const char *arg = argv[i];
        if (arg && arg[0] == '-') {
            run &= parse_option(argc, argv, &i, &opt);
        } else if (arg) file = arg;
    }

    cli_applyoptions(opt);

    if (run) {
        clidebugger_initialize();
        if (i < argc) morpho_setargs(argc - i, argv + i); // Pass unused args to morpho
        const char *preamble = (evalcode.count > 0) ? evalcode.data : NULL;
        
        if (file) {
            cli_run(file, opt, preamble);
        } else if (!inline_checktty()) {
            // stdin is piped/redirected - read and execute from stdin
            char *src = cli_loadstdin();
            if (src || preamble) {
                cli_runstring(src, opt, preamble);
                if (src) MORPHO_FREE(src);
            }
        } else if (preamble && !(opt & CLI_INTERACTIVE)) {
            // -e alone on a TTY: run the snippet and exit
            cli_runstring(preamble, opt, NULL);
        } else {
            // stdin is a TTY - enter REPL (optionally after -e preamble)
            cli(opt, preamble);
        }
    }

    varray_charclear(&evalcode);
    morpho_finalize();
    return cli_exitcode();
}
