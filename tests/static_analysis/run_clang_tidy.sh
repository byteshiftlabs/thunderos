#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Generate compile_commands.json for clang-tidy
# This tells clang-tidy the correct compiler flags and include paths

cd "$ROOT_DIR"

INCLUDE_DIR="./include"
CFLAGS="-march=rv64gc -mabi=lp64d -mcmodel=medany -nostdlib -nostartfiles -ffreestanding -fno-common -O0 -g -Wall -Wextra -I$INCLUDE_DIR"

echo "Generating compile_commands.json..."

cat > compile_commands.json << 'EOF'
[]
EOF

echo "[" > compile_commands.json
first=true

mapfile -t source_files < <(find kernel external/userland tests -name "*.c" -type f | sort)

if [ "${#source_files[@]}" -eq 0 ]; then
  echo "❌ No source files found"
  exit 1
fi

for file in "${source_files[@]}"; do
    if [ "$first" = false ]; then
        echo "," >> compile_commands.json
    fi
    first=false
    
    cat >> compile_commands.json << ENTRY
  {
    "directory": "$PWD",
    "command": "riscv64-unknown-elf-gcc $CFLAGS -c $file -o /dev/null",
    "file": "$file"
  }
ENTRY
done

echo "]" >> compile_commands.json

# Verify JSON is valid
if python3 -m json.tool compile_commands.json > /dev/null 2>&1; then
    echo "✅ compile_commands.json generated successfully"
else
    echo "❌ Invalid JSON in compile_commands.json"
    exit 1
fi

echo ""
echo "Running clang-tidy analysis..."
echo ""

# Run clang-tidy on all files
printf '%s\n' "${source_files[@]}" | xargs clang-tidy --header-filter=".*" 2>&1 | tee clang_tidy_analysis.txt

error_count=$(grep -Ec '^[^[:space:]].*:[0-9]+:[0-9]+: error:' clang_tidy_analysis.txt || true)
warning_count=$(grep -Ec '^[^[:space:]].*:[0-9]+:[0-9]+: warning:' clang_tidy_analysis.txt || true)

# Extract summary
echo ""
echo "=== ANALYSIS SUMMARY ==="
echo ""
echo "Real errors (from source code):"
echo "$error_count"
echo ""
echo "Real warnings (from source code):"
echo "$warning_count"
echo ""
echo "Unique error types:"
grep -E '^[^[:space:]].*:[0-9]+:[0-9]+: error:' clang_tidy_analysis.txt | sed 's/.*\[\(.*\)\].*/\1/' | sort | uniq -c | sort -rn | head -10 || true
echo ""
echo "Unique warning types:"
grep -E '^[^[:space:]].*:[0-9]+:[0-9]+: warning:' clang_tidy_analysis.txt | sed 's/.*\[\(.*\)\].*/\1/' | sort | uniq -c | sort -rn | head -10 || true

if [ "$error_count" -ne 0 ] || [ "$warning_count" -ne 0 ]; then
  exit 1
fi
