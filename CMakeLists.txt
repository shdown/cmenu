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

file (GLOB common_sources "*.cpp" "common/*.cpp")

set (MY_LATENCY_DEBUG_DEFINES)
option (WITH_LATENCY_DEBUG "With latency debug feature" OFF)
if (${WITH_LATENCY_DEBUG})
    set (MY_LATENCY_DEBUG_DEFINES -DWITH_LATENCY_DEBUG=1)
endif ()

option (WITHOUT_HARDENING "Without hardening" OFF)
if (${WITHOUT_HARDENING})
    set (MY_HARDENING_DEFINES)
else ()
    set (MY_HARDENING_DEFINES -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
endif ()

include (CheckSymbolExists)
set (CMAKE_REQUIRED_DEFINITIONS "-D_XOPEN_SOURCE=500")
check_symbol_exists (wcwidth "wchar.h" HAVE_WCWIDTH)
set (CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
check_symbol_exists (ppoll "poll.h" HAVE_PPOLL)
configure_file ("probes.in.hpp" "probes.generated.hpp")

set (CURSES_NEED_NCURSES TRUE)
set (CURSES_NEED_WIDE TRUE)
find_package (Curses)

find_library (MATH_LIBRARY m)

include (GNUInstallDirs)

function (my_configure_executable executable_name)
    target_compile_definitions (
        ${executable_name}
        PUBLIC
        ${MY_HARDENING_DEFINES}
        ${MY_LATENCY_DEBUG_DEFINES}
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

file (GLOB cmenu_sources "app_cmenu/*.cpp")
add_executable ("cmenu" ${common_sources} ${cmenu_sources})
my_configure_executable ("cmenu")

file (GLOB cmlauncher_sources "app_cmlauncher/*.cpp")
add_executable ("cmlauncher" ${common_sources} ${cmlauncher_sources})
my_configure_executable ("cmlauncher")

file (GLOB cmdesktopmenu_sources "app_cmdesktopmenu/*.cpp")
add_executable ("cmdesktopmenu" ${common_sources} ${cmdesktopmenu_sources})
my_configure_executable ("cmdesktopmenu")

find_package (PkgConfig REQUIRED)
pkg_check_modules (GLIB_STUFF REQUIRED glib-2.0 gio-unix-2.0)

target_include_directories (cmdesktopmenu SYSTEM PUBLIC ${GLIB_STUFF_INCLUDE_DIRS})
target_compile_options (cmdesktopmenu PUBLIC ${GLIB_STUFF_CFLAGS_OTHER})
target_link_libraries (cmdesktopmenu PUBLIC ${GLIB_STUFF_LIBRARIES})
