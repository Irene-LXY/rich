#!/usr/bin/env sh

fail=0

for file in testcases/iteration1/*.json; do
    echo "=================================================="
    echo "RUN: $file"
    ./rich_test "$file"
    if [ "$?" -ne 0 ]; then
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "RESULT: at least one case is not PASS"
    exit 1
fi

echo "RESULT: all cases PASS"
exit 0
