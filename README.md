![Morpho](https://github.com/Morpho-lang/morpho-manual/blob/main/src/Figures/morphologosmall-white.png#gh-light-mode-only)![Morpho](https://github.com/Morpho-lang/morpho-manual/blob/main/src/Figures/morphologosmall-white.png#gh-dark-mode-only)

# Morpho-cli

A terminal app for the [morpho language](https://github.com/Morpho-lang/morpho).  Improved unicode support requires an external library e.g. [libgrapheme](https://libs.suckless.org/libgrapheme/) or [libunistring](https://www.gnu.org/software/libunistring/).

Like morpho itself, Morpho-cli is released under an MIT license (see the LICENSE file for details). See the [main morpho repository](https://github.com/Morpho-lang/morpho) for information about contributing etc. Please report any issues or feature requests about the morpho-cli terminal app specifically here: suggestions for improvement are very welcome. 

## Installation

For this release, morpho can be installed on all supported platforms using the homebrew package manager. Alternatively, the program can be installed from source as described below. 

### Install with homebrew

The simplest way to install morpho-cli is through the [homebrew package manager](https://brew.sh). To do so:

1. If not already installed, install homebrew on your machine as described on the [homebrew website](https://brew.sh)

2. Open a terminal and type:

```
brew update
brew tap morpho-lang/morpho
brew install morpho morpho-cli morpho-morphoview morpho-morphopm
```

If you need to uninstall morpho, simply open a terminal and type `brew uninstall morpho-cli morpho-morphoview morpho`. It's very important to uninstall the homebrew morpho in this way before attempting to install from source as below.

### Install from source

To install, clone this repository:

    git clone https://github.com/Morpho-lang/morpho-cli.git

and then,

    cd morpho-cli 
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make install 

You may need to use sudo make install.

To run,

    morpho6
