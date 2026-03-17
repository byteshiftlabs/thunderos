#!/bin/bash
#
# ThunderOS Test Runner
# A visually appealing test runner with progress tracking
#
# Usage: ./test_runner.sh [OPTIONS]
#   --quick    Run only boot test (faster, ~5s)
#   --help     Show this help message
#
# Output is saved to tests/outputs/test_results.log
#
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/run_all_tests.sh" "$@"
