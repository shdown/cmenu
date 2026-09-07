cmake_minimum_required (VERSION 3.0...3.10)
project (cmenu CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_RTTI OFF)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif ()

set (common_sources

    app.cpp
    decode.cpp
    truncated_text.cpp
    int_utils.cpp
    style.cpp
    poll_utils.cpp
    parse_args.cpp
    my_assert.cpp
    fd_utils.cpp
)

set (XXX_HARDENING_DEFINES -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)

include (CheckSymbolExists)
set (CMAKE_REQUIRED_DEFINITIONS "-D_XOPEN_SOURCE=500")
check_symbol_exists (wcwidth "wchar.h" HAVE_WCWIDTH)
set (CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
check_symbol_exists (ppoll "poll.h" HAVE_PPOLL)
configure_file ("probes.in.h" "probes.generated.h")

set (CURSES_NEED_NCURSES TRUE)
set (CURSES_NEED_WIDE TRUE)
find_package (Curses)

find_library (MATH_LIBRARY m)

include (GNUInstallDirs)

function (my_add_executable executable_name main_file)
    add_executable (${executable_name} ${common_sources} ${main_file})

    target_compile_definitions (
        ${executable_name}
        PUBLIC
        ${XXX_HARDENING_DEFINES}
        -D_POSIX_C_SOURCE=200809L
    )

    target_include_directories (${executable_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

    target_link_libraries (${executable_name} PUBLIC ${CURSES_LIBRARIES})
    target_include_directories (${executable_name} PUBLIC ${CURSES_INCLUDE_DIRS})
    target_compile_options (${executable_name} PUBLIC ${CURSES_CFLAGS})

    if (MATH_LIBRARY)
        target_link_libraries (${executable_name} PUBLIC ${MATH_LIBRARY})
    endif ()

    install (TARGETS ${executable_name} DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction ()

my_add_executable ("cmenu" "cmenu.cpp")
my_add_executable ("cmlauncher" "cmlauncher.cpp")
