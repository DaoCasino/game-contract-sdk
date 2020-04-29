#! /bin/bash

set -o pipefail

. "${BASH_SOURCE[0]%/*}/utils.sh"

log "=========== Running unit tests ==========="

(
    tests=$(find ./build/examples/*/tests/ -maxdepth 1 -name "*_unit_test")
    for unit_test in $tests; do
        log "Running $unit_test..."
        $unit_test $@
    done
)
