#-------------------------------------------------------------------------------
# Morpho-cli/CMakeLists.txt
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)

project(morpho-cli LANGUAGES C)

include(GNUInstallDirs)

# Default to Release when the generator has a single build type.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "CMAKE_BUILD_TYPE unset; defaulting to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(morpho6 "")

find_package(morpho CONFIG QUIET)

# Compatibility shim for Morpho builds that predate the CMake package.
if(NOT TARGET morpho::morpho)
    message(STATUS "morpho CMake package not found; searching likely install paths")

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
              "C:/Program Files/Morpho/include/morpho"
              )
    if(NOT MORPHO_HEADER)
        unset(MORPHO_HEADER CACHE)
        message(FATAL_ERROR
            "Could not find Morpho. Install a Morpho that provides morphoConfig.cmake, "
            "or install morpho.h and libmorpho in a standard location.")
    endif()
    message(STATUS "Found morpho.h at ${MORPHO_HEADER}")

    # Identify folder that morpho.h is located in from MORPHO_HEADER and store in MORPHO_INCLUDE
    get_filename_component(MORPHO_INCLUDE ${MORPHO_HEADER} DIRECTORY)

    set(_morpho_includes ${MORPHO_INCLUDE})
    message(STATUS "Including directory ${MORPHO_INCLUDE}")

    # Add morpho headers in subfolders to MORPHO_INCLUDE
    file(GLOB morpho_subdirectories LIST_DIRECTORIES true ${MORPHO_INCLUDE}/*)
    foreach(dir ${morpho_subdirectories})
        if(IS_DIRECTORY ${dir})
            list(APPEND _morpho_includes ${dir})
            message(STATUS "Including directory ${dir}")
        endif()
    endforeach()

    # Locate libmorpho
    find_library(MORPHO_LIBRARY
        NAMES morpho libmorpho
        HINTS
        "C:/Program Files/Morpho/lib/"
    )
    if(NOT MORPHO_LIBRARY)
        unset(MORPHO_LIBRARY CACHE)
        message(FATAL_ERROR
            "Found morpho.h at ${MORPHO_HEADER} but could not find the morpho library.")
    endif()
    message(STATUS "Found morpho library at ${MORPHO_LIBRARY}")

    add_library(morpho::morpho UNKNOWN IMPORTED)
    set_target_properties(morpho::morpho PROPERTIES
        IMPORTED_LOCATION "${MORPHO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${_morpho_includes}"
    )

    unset(_morpho_includes)
    unset(morpho_subdirectories)
    unset(dir)
endif()

target_link_libraries(morpho6 PRIVATE morpho::morpho)

# Copy morpho.dll next to the executable.
get_target_property(_morpho_type morpho::morpho TYPE)
if(WIN32 AND _morpho_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET morpho6 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:morpho::morpho>
                $<TARGET_FILE_DIR:morpho6>
        VERBATIM)
endif()
unset(_morpho_type)

#-------------------------------------------------------------------------------
# Optional grapheme splitting library
#-------------------------------------------------------------------------------

set(_cli_grapheme FALSE)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBGRAPHEME QUIET IMPORTED_TARGET libgrapheme)
    if(TARGET PkgConfig::LIBGRAPHEME)
        target_link_libraries(morpho6 PRIVATE PkgConfig::LIBGRAPHEME)
        target_compile_definitions(morpho6 PRIVATE CLI_USELIBGRAPHEME)
        set(_cli_grapheme TRUE)
        message(STATUS "Found libgrapheme for grapheme splitting.")
    else()
        pkg_check_modules(LIBUNISTRING QUIET IMPORTED_TARGET libunistring)
        if(TARGET PkgConfig::LIBUNISTRING)
            target_link_libraries(morpho6 PRIVATE PkgConfig::LIBUNISTRING)
            target_compile_definitions(morpho6 PRIVATE CLI_USELIBUNISTRING)
            set(_cli_grapheme TRUE)
            message(STATUS "Found libunistring for grapheme splitting.")
        endif()
    endif()
endif()

if(NOT _cli_grapheme)
    find_path(GRAPHEME_INCLUDE NAMES grapheme.h)
    find_library(GRAPHEME_LIBRARY NAMES grapheme libgrapheme)
    if(GRAPHEME_INCLUDE AND GRAPHEME_LIBRARY)
        target_include_directories(morpho6 PRIVATE ${GRAPHEME_INCLUDE})
        target_link_libraries(morpho6 PRIVATE ${GRAPHEME_LIBRARY})
        target_compile_definitions(morpho6 PRIVATE CLI_USELIBGRAPHEME)
        set(_cli_grapheme TRUE)
        message(STATUS "Found libgrapheme for grapheme splitting.")
    else()
        find_path(UNISTRING_INCLUDE NAMES unigbrk.h)
        find_library(UNISTRING_LIBRARY NAMES unistring libunistring)
        if(UNISTRING_INCLUDE AND UNISTRING_LIBRARY)
            target_include_directories(morpho6 PRIVATE ${UNISTRING_INCLUDE})
            target_link_libraries(morpho6 PRIVATE ${UNISTRING_LIBRARY})
            target_compile_definitions(morpho6 PRIVATE CLI_USELIBUNISTRING)
            set(_cli_grapheme TRUE)
            message(STATUS "Found libunistring for grapheme splitting.")
        endif()
    endif()
endif()

if(NOT _cli_grapheme)
    message(STATUS "No grapheme splitting library found")
endif()
unset(_cli_grapheme)

#-------------------------------------------------------------------------------
# Include source files
#-------------------------------------------------------------------------------

add_subdirectory(src)

#-------------------------------------------------------------------------------
# Install targets
#-------------------------------------------------------------------------------

install(TARGETS morpho6 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
