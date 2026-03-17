#!/bin/bash
# Structured test output helpers for ThunderOS test scripts.
#
# Source this file from other test scripts:
#   source "$(dirname "$0")/structured_test_helpers.sh"
#
# Public API:
#   testfmt_run NAME PATTERN FILE        pass if PATTERN found in FILE
#   testfmt_run_absent NAME PATTERN FILE pass if PATTERN not found in FILE
#   testfmt_suite_begin NAME             print suite header
#   testfmt_suite_end NAME               print suite footer
#   testfmt_select_qemu [MIN_MAJOR]      choose a compatible qemu-system-riscv64
#   testfmt_summary [ELAPSED]            print summary; returns 1 if any failed
#
# Shared counters (reset on source):
#   _TF_PASS, _TF_FAIL, _TF_FAILED[]

export TERM="${TERM:-dumb}"

if [ -t 1 ] && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    _R='\033[0;31m' _G='\033[0;32m' _B='\033[0;34m' _N='\033[0m'
else
    _R='' _G='' _B='' _N=''
fi

_TF_PASS=0
_TF_FAIL=0
_TF_FAILED=()

testfmt_run() {
    local name="$1" pat="$2" f="$3"
    printf "[ RUN      ] %s\n" "$name"
    if grep -qE "$pat" "$f" 2>/dev/null; then
        printf "[${_G}       OK ${_N}] %s\n" "$name"
        _TF_PASS=$((_TF_PASS + 1))
        return 0
    fi
    printf "  expected pattern not found: %s\n" "$pat"
    printf "  last lines of captured output:\n"
    tail -8 "$f" 2>/dev/null | sed 's/^/    /'
    printf "[${_R}  FAILED  ${_N}] %s\n" "$name"
    _TF_FAIL=$((_TF_FAIL + 1))
    _TF_FAILED+=("$name")
    return 1
}

testfmt_run_absent() {
    local name="$1" pat="$2" f="$3"
    printf "[ RUN      ] %s\n" "$name"
    if ! grep -qEi "$pat" "$f" 2>/dev/null; then
        printf "[${_G}       OK ${_N}] %s\n" "$name"
        _TF_PASS=$((_TF_PASS + 1))
        return 0
    fi
    printf "  forbidden pattern found: %s\n" "$pat"
    grep -i "$pat" "$f" 2>/dev/null | head -5 | sed 's/^/    /'
    printf "[${_R}  FAILED  ${_N}] %s\n" "$name"
    _TF_FAIL=$((_TF_FAIL + 1))
    _TF_FAILED+=("$name")
    return 1
}

testfmt_suite_begin() {
    printf "[${_B}----------${_N}] Tests from %s\n" "$1"
}

testfmt_suite_end() {
    printf "[${_B}----------${_N}] %s\n\n" "$1"
}

testfmt_select_qemu() {
    local min_major="${1:-10}"
    local version_line version_major candidate
    local candidates=()

    if [ -n "${QEMU_BIN:-}" ]; then
        candidates+=("${QEMU_BIN}")
    fi
    if [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
        candidates+=("/tmp/qemu-10.1.2/build/qemu-system-riscv64")
    fi
    if command -v qemu-system-riscv64 >/dev/null 2>&1; then
        candidates+=("$(command -v qemu-system-riscv64)")
    fi

    for candidate in "${candidates[@]}"; do
        [ -n "$candidate" ] || continue
        [ -x "$candidate" ] || continue

        version_line="$($candidate --version 2>/dev/null | head -n 1)"
        version_major="$(printf '%s\n' "$version_line" | sed -n 's/.*version \([0-9][0-9]*\)\..*/\1/p')"
        if [ -n "$version_major" ] && [ "$version_major" -ge "$min_major" ]; then
            QEMU_BIN="$candidate"
            return 0
        fi
    done

    printf "[${_R}  ERROR   ${_N}] Compatible qemu-system-riscv64 not found (need major version >= %s).\n" "$min_major" >&2
    if command -v qemu-system-riscv64 >/dev/null 2>&1; then
        printf "[${_R}  ERROR   ${_N}] Host qemu-system-riscv64 is %s\n" "$(qemu-system-riscv64 --version | head -n 1)" >&2
    fi
    printf "[${_R}  ERROR   ${_N}] Set QEMU_BIN to a supported binary or run the Docker validation path.\n" >&2
    return 1
}

testfmt_export_counts() {
    printf 'PASS %d\nFAIL %d\n' "$_TF_PASS" "$_TF_FAIL" > "$1"
}

testfmt_summary() {
    local elapsed="${1:-0}" total=$((_TF_PASS + _TF_FAIL))
    printf "[${_B}==========${_N}] %d test(s) ran. (%ss total)\n" "$total" "$elapsed"
    printf "[${_G}  PASSED  ${_N}] %d test(s).\n" "$_TF_PASS"
    if [ "$_TF_FAIL" -gt 0 ]; then
        printf "[${_R}  FAILED  ${_N}] %d test(s), listed below:\n" "$_TF_FAIL"
        for t in "${_TF_FAILED[@]}"; do
            printf "[${_R}  FAILED  ${_N}] %s\n" "$t"
        done
        return 1
    fi
    return 0
}