
cmake_minimum_required(VERSION 3.24)
project(morpho-cli)

add_executable(morpho6 cli.c help.c linedit.c main.c)

find_library(MORPHO_LIBRARY
    NAMES morpho libmorpho
)

target_include_directories(morpho6 PUBLIC /usr/local/include/morpho)
target_link_libraries(morpho6 ${MORPHO_LIBRARY})

install(TARGETS morpho6)