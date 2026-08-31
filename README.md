![Morpho](https://github.com/Morpho-lang/morpho-manual/blob/main/src/Figures/morphologosmall-white.png#gh-light-mode-only)![Morpho](https://github.com/Morpho-lang/morpho-manual/blob/main/src/Figures/morphologosmall-white.png#gh-dark-mode-only)

# Morpho-cli

A terminal app for the [morpho language](https://github.com/Morpho-lang/morpho) using the [inline line editor](https://github.com/Morpho-lang/inline).  Improved unicode support requires an external library e.g. [libgrapheme](https://libs.suckless.org/libgrapheme/) or [libunistring](https://www.gnu.org/software/libunistring/).

Like morpho itself, Morpho-cli is released under an MIT license (see the LICENSE file for details). See the [main morpho repository](https://github.com/Morpho-lang/morpho) for information about contributing etc. Please report any issues or feature requests about the morpho-cli terminal app specifically here: suggestions for improvement are very welcome. 

## Installation

The simplest way to install the morpho terminal app is to use the [homebrew package manager](https://brew.sh). To do so:

1. If not already installed, install homebrew on your machine as described on the [homebrew website](https://brew.sh)

2. Open a terminal and type:

```
brew update
brew tap morpho-lang/morpho
brew install morpho morpho-cli morpho-morphoview morpho-morphopm
```

If you need to uninstall morpho, simply open a terminal and type `brew uninstall morpho-cli morpho-morphoview morpho`. It's very important to uninstall the homebrew morpho in this way before attempting to install from source as below.

## Running

To run type, 

    morpho6

into a terminal. The terminal app takes a number of optional arguments:

| Option | Description |
|--------|-------------|
| `-h`, `--help [query]` | Show CLI help, or look up a language help topic |
| `-v`, `--version` | Show version information |
| `-c`, `--check` | Check syntax without executing |
| `-e`, `--eval <code>` | Execute a code string (before file/REPL if combined) |
| `-i`, `--interactive` | Enter REPL after running a file |
| `-l`, `--list <file>` | List a file with syntax highlighting |
| `-d`, `--disassemble` | Show disassembly |
| `-D` | Disassemble only (no execution) |
| `-dl` | Disassemble with source listing |
| `-debug`, `--debug` | Enable debugger |
| `-O`, `--optimize` | Enable optimizations |
| `-profile`, `--profile` | Enable profiling |
| `--no-color` | Disable syntax highlighting |
| `--no-hints` | Disable hint output |
| `-w`, `--workers <n>` | Set number of worker threads |

If no file is specified, morpho enters interactive mode. If stdin is piped or redirected, morpho reads and executes from stdin. Any options after the file name are passed to the morpho program and are available from the `System` class:

    var args = System.arguments()

## Manual install from source

To install, clone this repository:

    git clone https://github.com/Morpho-lang/morpho-cli.git

and then,

    cd morpho-cli 
    cmake -S . -B build
    cmake --build build --config Release
    sudo cmake --install build --config Release

This manual build installs into '/usr/local/bin' by default.
