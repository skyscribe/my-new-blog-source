#!/bin/bash
total=0
for fname in $(find _deploy/post -type f -a -name "*.html"); do
    count=$(cat "${fname}" | sed -n  "s/.*本文有\s*\([0-9]\+\).*/\1/gp")
    if [ -n "$count" ]; then
        total=$(($total+$count))
    else
        echo "no count for ", $fname
    fi
done
echo "Total words: $total"
