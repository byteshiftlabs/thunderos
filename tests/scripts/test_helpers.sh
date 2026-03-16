#!/bin/bash
# gtest-style output helpers for ThunderOS test scripts.
#
# Source this file from other test scripts:
#   source "$(dirname "$0")/test_helpers.sh"
#
# Public API:
#   gtest_run NAME PATTERN FILE        pass if PATTERN found in FILE
#   gtest_run_absent NAME PATTERN FILE pass if PATTERN not found in FILE
#   gtest_suite_begin NAME             print suite header
#   gtest_suite_end NAME               print suite footer
#   gtest_summary [ELAPSED]            print summary; returns 1 if any failed
#
# Shared counters (reset on source):
#   _GT_PASS, _GT_FAIL, _GT_FAILED[]

export TERM="${TERM:-dumb}"

if [ -t 1 ] && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    _R='\033[0;31m' _G='\033[0;32m' _B='\033[0;34m' _N='\033[0m'
else
    _R='' _G='' _B='' _N=''
fi

_GT_PASS=0
_GT_FAIL=0
_GT_FAILED=()

gtest_run() {
    local name="$1" pat="$2" f="$3"
    printf "[ RUN      ] %s\n" "$name"
    if grep -q "$pat" "$f" 2>/dev/null; then
        printf "[${_G}       OK ${_N}] %s\n" "$name"
        _GT_PASS=$((_GT_PASS + 1))
        return 0
    fi
    printf "  expected pattern not found: %s\n" "$pat"
    printf "  last lines of captured output:\n"
    tail -8 "$f" 2>/dev/null | sed 's/^/    /'
    printf "[${_R}  FAILED  ${_N}] %s\n" "$name"
    _GT_FAIL=$((_GT_FAIL + 1))
    _GT_FAILED+=("$name")
    return 1
}

# Passes when PATTERN is NOT found — used for panic/error checks.
gtest_run_absent() {
    local name="$1" pat="$2" f="$3"
    printf "[ RUN      ] %s\n" "$name"
    if ! grep -qi "$pat" "$f" 2>/dev/null; then
        printf "[${_G}       OK ${_N}] %s\n" "$name"
        _GT_PASS=$((_GT_PASS + 1))
        return 0
    fi
    printf "  forbidden pattern found: %s\n" "$pat"
    grep -i "$pat" "$f" 2>/dev/null | head -5 | sed 's/^/    /'
    printf "[${_R}  FAILED  ${_N}] %s\n" "$name"
    _GT_FAIL=$((_GT_FAIL + 1))
    _GT_FAILED+=("$name")
    return 1
}

gtest_suite_begin() {
    printf "[${_B}----------${_N}] Tests from %s\n" "$1"
}

gtest_suite_end() {
    printf "[${_B}----------${_N}] %s\n\n" "$1"
}

# Print final summary. Returns 1 if any test failed.
gtest_summary() {
    local elapsed="${1:-0}" total=$((_GT_PASS + _GT_FAIL))
    printf "[${_B}==========${_N}] %d test(s) ran. (%ss total)\n" "$total" "$elapsed"
    printf "[${_G}  PASSED  ${_N}] %d test(s).\n" "$_GT_PASS"
    if [ "$_GT_FAIL" -gt 0 ]; then
        printf "[${_R}  FAILED  ${_N}] %d test(s), listed below:\n" "$_GT_FAIL"
        for t in "${_GT_FAILED[@]}"; do
            printf "[${_R}  FAILED  ${_N}] %s\n" "$t"
        done
        return 1
    fi
    return 0
}
