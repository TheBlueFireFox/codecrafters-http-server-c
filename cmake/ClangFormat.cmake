# cmake/ClangFormat.cmake

find_program(CLANG_FORMAT
    NAMES clang-format
    REQUIRED
)

function(glob_files OUT_VAR)
    cmake_parse_arguments(
        ARG
        ""
        ""
        "EXTENSIONS;PATHS"
        ${ARGN}
    )

    if(NOT ARG_EXTENSIONS)
        message(FATAL_ERROR "glob_files: EXTENSIONS must be specified")
    endif()

    if(NOT ARG_PATHS)
        message(FATAL_ERROR "glob_files: PATHS must be specified")
    endif()

    set(patterns)

    foreach(path IN LISTS ARG_PATHS)
        foreach(ext IN LISTS ARG_EXTENSIONS)
            list(APPEND patterns "${path}/*.${ext}")
        endforeach()
    endforeach()

    file(GLOB files
        CONFIGURE_DEPENDS
        ${patterns}
    )

    set(${OUT_VAR} ${files} PARENT_SCOPE)
endfunction()

set(SOURCE_EXTENSIONS
    c
    cc
    cpp
    cxx
    h
    hh
    hpp
    hxx
)


glob_files(PROJECT_SOURCE_FILES
    EXTENSIONS 
        ${SOURCE_EXTENSIONS}
    PATHS
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/test"
        "${PROJECT_SOURCE_DIR}/benchmark"
)

add_custom_target(format
    COMMAND
        "${CLANG_FORMAT}"
        -i
        -style=file
        ${PROJECT_SOURCE_FILES}
    COMMENT "Formatting C/C++ sources"
    VERBATIM
)

add_custom_target(format-check
    COMMAND
        "${CLANG_FORMAT}"
        --dry-run
        --Werror
        -style=file
        ${CLANG_FORMAT_FILES}
    COMMENT "Checking C/C++ formatting"
    VERBATIM
)
