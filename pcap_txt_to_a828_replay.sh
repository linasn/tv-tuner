#!/bin/bash
[[ $# -ne 2 ]] && echo "Usage: $0 input_file output_file" && exit 1
sed -n '/Time/d;/^$/d;s/ *[0-9]* \([\.0-9]*\) *host.*/[\1]/p;s/\(00[4-9]0  [ 0-9a-f]* \).*/\1/p' $1 |
awk '{$1=$1};1' | sed ':t;$!N;/\n00[567]0/s///;tt;s/^0040 //;P;D' > $2
