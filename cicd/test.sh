#! /bin/bash

set -o pipefail

. "${BASH_SOURCE[0]%/*}/utils.sh"

is_debug=y

usage() {
  echo "Test contracts."
  echo
  echo "Usage:"
  echo
  echo "  $PROGNAME [ OPTIONS ]"
  echo
  echo "Options:"
  echo
  echo "  --release              : is release"
  echo
  echo "  -h, --help           : print this message"
}

OPTS="$( getopt -o "h" -l "\
release,\
help" -n "$PROGNAME" -- "$@" )"
eval set -- "$OPTS"
while true; do
  case "${1:-}" in
  (--release)      is_debug=n      ; shift   ; readonly is_debug ;;
  (-h|--help)      usage ; exit 0 ;;
  (--)             shift ; break ;;
  (*)              die "Invalid option: ${1:-}." ;;
  esac
done
unset OPTS

log "=========== Running unit tests ==========="

(
    tests=$(find ./build/examples/*/tests/ -maxdepth 1 -name "*_unit_test")
    for unit_test in $tests; do
        log "Running $unit_test..."
        $unit_test $@
    done
)
