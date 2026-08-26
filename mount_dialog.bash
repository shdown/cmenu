#!/usr/bin/env bash

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

tool_available() {
    command -v "$1" >/dev/null
}

declare -A devs=()
declare -A dev_attrs=()

maybe_pause() {
    local junk
    if (( pause_flag )); then
        echo >&2 "Press Enter to continue..."
        IFS= read -r junk
    fi
}

fail_fatal_error() {
    maybe_pause
    exit 1
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
    if (( ${#devs[@]} == 0 )); then
        return
    fi
    while IFS= read -r line; do
        dev_list+=("$line")
    done < <(printf '%s\n' "${!devs[@]}" | sort)
}
fill_dev_list

if (( ${#dev_list[@]} == 0 )); then
    echo >&2 'No devices found!'
    fail_fatal_error
fi

say_sanitized() {
    local s=$1
    s=${s//$'\n'/}
    s=${s//$'\r'/}
    printf '%s\n' "$s"
}

gen_input_real() {
    local x
    printf '%s\n' "n ${#dev_list[@]}" || return $?
    for x in "${dev_list[@]}"; do
        printf '+\n' || return $?
        say_sanitized "${dev_attrs[label:$x]}" || return $?
        say_sanitized "${dev_attrs[partlabel:$x]}" || return $?
        say_sanitized "$x" || return $?
    done
}

gen_input() {
    # We don't want to terminate if a write failed with SIGPIPE here, so let's fork off.
    # This function doesn't modify any globals anyway.
    ( gen_input_real; )
}

output_type=
output_value=

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

coproc cmenu \
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

if [[ "$output_type" != result ]]; then
    exit 0
fi

dev=${dev_list[$output_value]}
echo >&2 "Will mount device: $dev"

IFS= read -r -p 'Mountpoint: ' mp || exit $?

if [[ $mp != /* ]]; then
    echo >&2 "E: mountpoint is not an absolute path."
    fail_fatal_error
fi

if [[ -L $mp ]]; then
    echo >&2 "E: refusing to mount on a symlink."
    fail_fatal_error
fi

if tool_available findmnt; then
    if findmnt --source "$dev" >/dev/null; then
        echo >&2 "E: device appears to be already mounted:"
        findmnt --source "$dev" >&2
        fail_fatal_error
    fi

    if findmnt --mountpoint "$mp" >/dev/null; then
        echo >&2 "E: something is already mounted at $mp."
        fail_fatal_error
    fi
fi

mount_opts=
if tool_available lsblk; then
    fs_type=$(lsblk -dn -o FSTYPE -- "$dev" | head -1) || fs_type=
    case "$fs_type" in
    vfat|msdos|exfat|ntfs|ntfs3)
        uid=$(id -u) || uid=0
        gid=$(id -g) || gid=0
        mount_opts="-o rw,uid=$((uid)),gid=$((gid)),dmask=022,fmask=133"
        ;;
    esac
fi

if ! sudo /bin/sh -c 'set -e; mkdir -p -- "$2"; mount $3 -- "$1" "$2"' _ "$dev" "$mp" "$mount_opts"; then
    fail_fatal_error
fi
