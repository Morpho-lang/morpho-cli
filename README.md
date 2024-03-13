# morpho-cli

A Terminal app for the [morpho language](https://github.com/Morpho-lang/morpho). Improved unicode support requires an external library e.g. [libgrapheme](https://libs.suckless.org/libgrapheme/) or [libunistring](https://www.gnu.org/software/libunistring/)

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
