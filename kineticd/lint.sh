#!/bin/bash

# Array of directories that should be linted
LINT_DIRS=(
    './src'
    './ha_zac_cmds'
    './lldp'
    './qual'
)

# Loop for linting all directories and capturing the exit status
EXIT_STATUS=0
for TO_LINT in "${LINT_DIRS[@]}"; do
    echo "Linting ${TO_LINT} files"
    find $TO_LINT \( -name "*.h" -o -name "*.cc" \) -type f | grep -v pb | xargs python cpplint.py --root=src --header-guard-prefix=KINETIC --filter=-legal/copyright,-build/include,-whitespace/comments,-readability/streams,-runtime/references,-readability/casting,-runtime/arrays,-runtime/printf
    ((EXIT_STATUS += $?))
done

# Exit using the exit status
exit $EXIT_STATUS
