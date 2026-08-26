#!/usr/bin/env bash

set -e

require_enter_after_combo=${1:-0}

gen_input() {
    printf '%s\n' 'N 5'
    printf '%s\n' '+' 'x' 'Xylophone' 'Musical instrument'
    printf '%s\n' '+' 'yy' 'Yuyuko' 'Touhou character'
    printf '%s\n' '+' 'yz' 'Yunnanozoan' 'Extinct animal'
    printf '%s\n' '+' 'a' '/a/ — Anime' 'Board'
    printf '%s\n' '+' 'aa' 'Alcoholic Anonymous' 'Organization'
}

output=$(gen_input | ${CMENU:-cmenu} \
    -mode=bare \
    -require-enter-after-combo="$require_enter_after_combo" \
    -column=@6:'Combo' \
    -column=:'Word' \
    -column=:'Description' \
    -combo=x \
    -combo=yy \
    -combo=yz \
    -combo=a \
    -combo=aa
)

echo "Output: '$output'"
