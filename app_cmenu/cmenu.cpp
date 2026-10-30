/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include <utility>
#include <exception>
#include <stdio.h>
#include "../common/fd.hpp"
#include "../app.hpp"
#include "../parse_args.hpp"
#include "../errors.hpp"
#include "../fd_utils.hpp"

int main(int argc, char **argv)
{
    Args my_args;
    try {
        parse_args(argc, argv, my_args);
    } catch (const CommandLineArgsError &e) {
        fprintf(stderr, "Wrong USAGE: %s\n", e.what());
        print_usage(stderr, argc == 0 ? "cmenu" : argv[0]);
        return 2;
    }

    try {

        std::pair<FD, FD> bare_mode_inout;
        if (my_args.bare_mode) {
            bare_mode_inout = fd_utils::copy_stdin_and_stdout_fds();
        }

        fd_utils::reset_stdin_and_stdout_fds();
        app::sigpipe_ignore();

        if (my_args.bare_mode) {
            app::run_bare_mode(
                std::move(my_args),
                std::move(bare_mode_inout.first),
                std::move(bare_mode_inout.second)
            );
        } else {
            app::run_normal_mode(std::move(my_args));
        }

    } catch (const CMenuError &e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Caught error: %s\n", e.what());
        return 1;
    }
    return 0;
}
