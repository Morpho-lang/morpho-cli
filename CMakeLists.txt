cmake_minimum_required(VERSION 3.13)
project(morpho-cli)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(morpho6 "") 
add_subdirectory(src)

# Locate the morpho.h header file and store in MORPHO_HEADER
find_file(MORPHO_HEADER
          morpho.h 
          HINTS 
          /usr/local/include/
          /usr/local/include/morpho
          /opt/homebrew/include/
          /opt/homebrew/include/morpho
          /home/linuxbrew/.linuxbrew/include/
          /home/linuxbrew/.linuxbrew/include/morpho
          )

# Identify folder that morpho.h is located in from MORPHO_HEADER and store in MORPHO_INCLUDE
get_filename_component(MORPHO_INCLUDE ${MORPHO_HEADER} DIRECTORY)

# Add morpho headers to MORPHO_INCLUDE
target_include_directories(morpho6 PUBLIC ${MORPHO_INCLUDE})

# Add morpho headers in subfolders to MORPHO_INCLUDE
file(GLOB morpho_subdirectories LIST_DIRECTORIES true ${MORPHO_INCLUDE}/*)
foreach(dir ${morpho_subdirectories})
    IF(IS_DIRECTORY ${dir})
        target_include_directories(morpho6 PUBLIC ${dir})
    ELSE()
        CONTINUE()
    ENDIF()
endforeach()

# Locate libmorpho
find_library(MORPHO_LIBRARY
    NAMES morpho libmorpho
)

# Try to find a grapheme splitting library
unset(GRAPHEME_LIBRARY CACHE)

# First, try to locate libgrapheme
find_library(GRAPHEME_LIBRARY
    NAMES grapheme libgrapheme
)

# Find grapheme.h header file 
find_file(GRAPHEME_HEADER grapheme.h 
          HINTS 
            /usr/local/include
            /opt/homebrew/include/
            /home/linuxbrew/.linuxbrew/include/)

if(GRAPHEME_LIBRARY)
    message("-- Found libgrapheme for grapheme splitting.")
    add_definitions( -DCLI_USELIBGRAPHEME )
else()
    # Locate libunistring
    find_library(GRAPHEME_LIBRARY
        NAMES unistring libunistring
    )

    # Find unigbrk.h header file 
    find_file(GRAPHEME_HEADER unigbrk.h 
            HINTS 
                /usr/local/include
                /opt/homebrew/include/
                /home/linuxbrew/.linuxbrew/include/)

    if(GRAPHEME_LIBRARY)
        message("-- Found libunistring for grapheme splitting.")
        add_definitions( -DCLI_USELIBUNISTRING )
    endif() 
endif() 

# Identify folder that the grapheme library is located in from GRAPHEME_HEADER and store in GRAPHEME_INCLUDE
get_filename_component(GRAPHEME_INCLUDE ${GRAPHEME_HEADER} DIRECTORY)

# Link the grapheme library if provided
if(GRAPHEME_LIBRARY)
    target_link_libraries(morpho6 ${MORPHO_LIBRARY} ${GRAPHEME_LIBRARY})
    target_include_directories(morpho6 PUBLIC ${GRAPHEME_INCLUDE})
else() 
    message("!! No grapheme splitting library found")
endif()

# Install the resulting binary
install(TARGETS morpho6)
