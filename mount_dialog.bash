#!/usr/bin/env bash

# This script expects:
#  * a GNU/Linux system with udev and /dev/disk/... directory hierarchy;
#  * a working 'sudo' binary.

shopt -s nullglob

pause_flag=0

print_usage_and_exit() {
    echo >&2 "USAGE: $0 [--pause]"
    exit 2
}

if (( $# != 0 )); then
    if (( $# != 1 )); then
        print_usage_and_exit
    fi
    if [[ $1 != --pause ]]; then
        print_usage_and_exit
    fi
    pause_flag=1
fi

declare -A devs=()
declare -A dev_attrs=()

maybe_pause() {
    local junk
    if (( pause_flag )); then
        echo >&2 "Press Enter to continue..."
        IFS= read -r junk
    fi
}

parse_attr() {
    local a=$1
    local s
    local f
    for s in /dev/disk/by-"$a"/*; do
        f=$(readlink -f -- "$s") || continue
        devs[$f]=1
        dev_attrs["$a:$f"]="${s##*/}"
    done
}

parse_attr label
parse_attr partlabel

declare -a dev_list=()

fill_dev_list() {
    local line
    while IFS= read -r line; do
        dev_list+=("$line")
    done < <(printf '%s\n' "${!devs[@]}" | sort)
}
fill_dev_list

if (( ${#dev_list[@]} == 0 )); then
    echo >&2 'No devices found!'
    maybe_pause
    exit 1
fi

gen_input_real() {
    local x
    printf '%s\n' "n ${#dev_list[@]}" || return $?
    for x in "${dev_list[@]}"; do
        printf '+\n' || return $?
        printf '%s\n' "${dev_attrs[label:$x]}" || return $?
        printf '%s\n' "${dev_attrs[partlabel:$x]}" || return $?
        printf '%s\n' "$x" || return $?
    done
}

gen_input() {
    # We don't want to terminate if a write failed with SIGPIPE here, so let's fork off.
    # This function doesn't modify any globals anyway.
    ( gen_input_real; )
}

declare output_type=
declare output_value=

handle_output() {
    local line
    output_type=
    output_value=
    while IFS= read -r line; do
        case "$line" in
        ok)
            ;;
        result)
            if IFS= read -r output_value; then
                output_type=result
            fi
            return 0
            ;;
        *)
            printf >&2 '%s\n' "WARNING: unsupported line in cmenu output: '$line'"
            ;;
        esac
    done
    return 0
}

x_me=$(readlink -- "$0" || printf '%s\n' "$0") || exit $?
x_mydir=$(dirname -- "$x_me") || exit $?
coproc "$x_mydir"/cmenu \
    -infd=3 \
    -outfd=4 \
    -column=:'Label' \
    -column=:'Partition label' \
    -column=:'Device' \
    3<&0 0</dev/null 4>&1 1>&2

gen_input >&${COPROC[1]}

handle_output <&${COPROC[0]}

if [[ -n "$COPROC_PID" ]]; then
    wait "$COPROC_PID"
fi

if [[ "$output_type" == result ]]; then
    dev=${dev_list[$output_value]}
    echo >&2 "Will mount device: $dev"
    IFS= read -r -p 'Mountpoint: ' mp || exit $?
    if [[ $mp != /* ]]; then
        echo >&2 "E: mountpoint is not an absolute path."
        maybe_pause
        exit 1
    fi
    if ! [[ -d $mp ]]; then
        if ! mkdir -- "$mp"; then
            maybe_pause
            exit 1
        fi
    fi
    if ! sudo mount "$dev" "$mp"; then
        maybe_pause
        exit 1
    fi
fi
