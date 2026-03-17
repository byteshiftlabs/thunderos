# ThunderOS Audit Findings

## Executive Summary
- Audit status: Final merge-readiness re-audit completed against the current branch and working tree
- Total blockers: 1 remaining
- Total serious issues: 0 remaining
- Total minor issues: 0 remaining
- Current recommendation: Do not merge until the superproject and userland submodule state are normalized into committed, reproducible revisions; reviewed code surfaces are otherwise mergeable
- Supported runtime verification: completed in Docker/QEMU 10.1.2 environment; current available suite passes 30/30 (25 kernel + 5 integration), `./run_os_docker.sh` reaches the shell prompt, and host `./run_os.sh` now fails fast on unsupported QEMU 6.2.0 instead of hanging
- Residual risk posture: no confirmed open code defects remain on the reviewed surfaces; the remaining merge blocker is repository-state hygiene (dirty superproject + dirty userland submodule). Architectural limits such as the global FD table remain documented follow-up work, not release blockers for the currently reviewed surface.

## Findings Table
| ID | Severity | Confidence | File | Issue | Recommended Fix | Status |
|----|----------|------------|------|-------|-----------------|--------|
| B1 | Blocker | confirmed | kernel/core/wait_queue.c | Lost-wakeup race on allocation failure path in wait_queue_sleep | Remove the schedule-on-ENOMEM path or make sleep queue insertion atomic without kmalloc in the critical path | fixed (build-verified) |
| B2 | Blocker | confirmed | kernel/core/rwlock.c | Reader lost-wakeup / deadlock risk in rwlock_read_lock | Make the check-and-sleep path atomic; do not drop interrupt protection before enqueueing on the wait queue | fixed (build-verified) |
| B3 | Blocker | confirmed | kernel/core/shell.c, kernel/core/elf_loader.c | Shell launches pass argv to elf_load_exec, but elf_load_exec discards argc/argv and never sets them on the new process | Wire argv/argc through the process creation path or switch shell launches to the execve-style loader path that already builds a user argv stack | fixed (build-verified, Docker/QEMU-verified) |
| B4 | Blocker | confirmed | repository root, userland/ | Superproject and userland submodule are still dirty/uncommitted, so the PR is not reproducible or merge-ready | Commit or reset the userland submodule, record the intended gitlink in the superproject, then rerun CI on a clean tree | open (merge hygiene) |
| S1 | Serious | confirmed | kernel/fs/ext2_vfs.c | ext2_vfs_write treats partial writes as success | Validate full-length writes or document/propagate partial-write semantics explicitly | fixed (build-verified) |
| S2 | Serious | confirmed | tests/unit/ | No unit or stress coverage for rwlock, mutex, condvar, or wait queue behavior | Add synchronization tests, especially for contention and wakeup behavior | fixed (build-verified) |
| S3 | Serious | confirmed | README.md | Public docs do not state known limits or support boundaries for release | Add a limitations / known constraints section before publishing | fixed |
| S4 | Serious | confirmed | kernel/drivers/fbconsole.c | fbcon_scroll_up underflows when the framebuffer is shorter than one glyph row, producing out-of-bounds framebuffer writes | Guard zero-row / short-framebuffer cases before subtracting FONT_HEIGHT and before clearing the last row | fixed (build-verified, Docker/QEMU-verified) |
| S5 | Serious | confirmed | kernel/drivers/framebuffer.c, kernel/drivers/virtio_gpu.c | Framebuffer size, pixel-count, and pixel-index arithmetic is unchecked and can overflow before allocation or access | Add checked multiplication/addition for width*height, width*height*4, y*width+x, and region clamp arithmetic before indexing or allocating | fixed (build-verified, Docker/QEMU-verified) |
| S6 | Serious | confirmed | kernel/drivers/virtio_blk.c | Block I/O bounds check uses sector + count directly, allowing unsigned wraparound to bypass capacity validation | Rewrite the capacity check as count > capacity - sector after validating sector <= capacity | fixed (build-verified, Docker/QEMU-verified) |
| S7 | Serious | confirmed | userland/core/chown.c, userland/core/sleep.c | Userland numeric parsing and time conversion overflow silently, causing wrapped UID/GID values and broken sleep intervals | Reject out-of-range numeric input and guard the seconds-to-milliseconds conversion against overflow | fixed (build-verified) |
| S8 | Serious | confirmed | userland/core/mkdir.c, userland/core/rmdir.c | mkdir and rmdir are shipped as placeholders that only print usage text and never invoke the directory syscalls | Implement real argv-based mkdir/rmdir behavior or stop advertising and shipping them as core utilities | fixed (build-verified) |
| M1 | Minor | confirmed | repository root | No SECURITY.md or vulnerability reporting path | Add SECURITY.md with reporting expectations and support policy | fixed |
| M2 | Minor | confirmed | .github/workflows/ci.yml | CI does not preserve test logs as artifacts for failure diagnosis | Upload test and analysis logs as workflow artifacts | fixed |
| M3 | Minor | confirmed | userland/net/udp_network_test.c | Network test uses stale socket syscall numbers and a mismatched recvfrom call signature, so it cannot exercise the intended kernel path reliably | Sync the syscall numbers with include/kernel/syscall.h and pass the receive address length explicitly | fixed (build-verified) |
| S9 | Serious | confirmed | userland/bin/ush.c, kernel/fs/vfs.c | Shell pipeline via fork+exec+dup2 hangs because global FD table lets dup2 on FDs 0-2 corrupt shared entries, process_exit never closes FDs, and sys_read/sys_write hardcode FDs 0-2 to vterm | Replace fork+exec pipeline with in-process builtin pipeline that uses pipe FDs directly; document global FD table as architectural limitation | fixed (Docker/QEMU-verified, 12/12 soak) |
| M4 | Minor | confirmed | tests/scripts/structured_test_helpers.sh | testfmt_run uses basic grep but test patterns contain pipe alternation requiring extended regex | Use grep -E (extended regex) in testfmt_run and testfmt_run_absent | fixed (Docker/QEMU-verified) |

## Detailed Findings

### Blockers

#### B1
- Location: `kernel/core/wait_queue.c`
- Confidence: confirmed
- Problem: `wait_queue_sleep()` disables interrupts, attempts to allocate a queue entry with `kmalloc()`, and on allocation failure restores interrupts, sets errno, and calls `schedule()` before the current process is ever linked into the wait queue.
- Why it matters: this is a lost-wakeup class bug. The caller can yield as if it were going to sleep, but no queue state exists to ensure a matching wakeup path. In a blocking kernel primitive this can turn memory pressure into hangs or inconsistent wake behavior.
- Evidence:

```c
wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
if (!entry) {
	interrupt_restore(old_state);
	set_errno(THUNDEROS_ENOMEM);
	schedule();
	return -1;
}
```

- Recommended fix: do not call `schedule()` on this ENOMEM path. Either return the error immediately and let callers decide how to retry, or redesign wait queue entries so the enqueue path does not depend on dynamic allocation while interrupts are disabled.
- Test gap: there is no test covering wait-queue behavior under allocation failure.
- Status update: fixed in code. `wait_queue_sleep()` now returns `THUNDEROS_ENOMEM` immediately on internal allocation failure, and a new kernel unit test covers the NULL-queue and forced-allocation-failure paths. Verified by building with `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a supported Docker/QEMU 10.1.2 test-mode boot.

#### B2
- Location: `kernel/core/rwlock.c`
- Confidence: confirmed
- Problem: `rwlock_read_lock()` checks `rw->writer || rw->writers_waiting > 0`, restores interrupts, then calls `wait_queue_sleep()` on the reader queue. The condition check and sleep are not atomic.
- Why it matters: this creates a classic lost-wakeup window. A writer can change the lock state between the condition check and the reader actually enqueueing itself, which can strand readers indefinitely.
- Evidence:

```c
while (rw->writer || rw->writers_waiting > 0) {
	interrupt_restore(flags);
	wait_queue_sleep(&rw->reader_queue);
	flags = interrupt_save_disable();
}
```

- Recommended fix: make the reader check-and-sleep transition atomic. The reader must either remain protected until it is on the wait queue, or use a wait primitive that couples condition evaluation with queue insertion.
- Test gap: no rwlock contention or lost-wakeup tests exist in `tests/unit/`.
- Status update: fixed in code. `rwlock_read_lock()` and `rwlock_write_lock()` now enqueue waiters with interrupts still disabled, then re-check the lock condition before sleeping so a release in that window cannot strand the waiter. Verified by building with `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a supported Docker/QEMU 10.1.2 test-mode boot.

#### B3
- Location: `kernel/core/shell.c`, `kernel/core/elf_loader.c`
- Confidence: confirmed
- Problem: the interactive shell parses command arguments and passes them to `elf_load_exec()`, but `elf_load_exec()` explicitly discards `argv` and `argc` and never builds a user argument vector for the new process.
- Why it matters: this breaks a large part of the advertised userland surface. Utilities such as `cat`, `rm`, `touch`, `sleep`, `chmod`, `chown`, and `kill` all expect `argc`/`argv`, but shell-launched processes do not receive them.
- Evidence:

```c
static int shell_exec_program(const char *program_path, int argument_count, char **argument_vector) {
	const char **const_argv = (const char **)argument_vector;
	int process_id = elf_load_exec(program_path, const_argv, argument_count);
```

```c
int elf_load_exec(const char *path, const char *argv[], int argc) {
	(void)argv;
	(void)argc;
```

- Recommended fix: route shell launches through the exec path that already constructs a user argv stack, or extend `elf_load_exec()` to initialize the new process stack and trap frame with `argc`/`argv` the same way `elf_exec_replace()` already does.
- Test gap: there is no end-to-end userland test that launches an external command with arguments from `ush` and asserts that `argc`/`argv` arrive intact.
- Status update: fixed in code. `elf_load_exec()` now forwards `argv` and `argc` into `process_create_elf()`, the process layer now lays out a real user-space argument stack for fresh ELF launches, and `tests/unit/test_elf.c` includes a regression that inspects the new process trap frame and user stack contents directly. Verified by `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a full `make test` run inside a freshly rebuilt Docker image using QEMU 10.1.2.

#### B4
- Location: `repository root`, `userland/`
- Confidence: confirmed
- Problem: the branch is still living in a dirty working tree. The superproject has broad unstaged/staged changes, `run_os_docker.sh` is not yet committed, and the `userland/` submodule itself has local modifications plus generated build output.
- Why it matters: merging a dirty tree is not engineering. CI and reviewers cannot reproduce exactly what is being shipped unless the superproject and submodule both point at committed revisions.
- Evidence:

```text
git status --short
...
Am userland
?? run_os_docker.sh

git -C userland status --short
 M bin/ush.c
 M core/chown.c
 M core/mkdir.c
 M core/rmdir.c
 M core/sleep.c
 M net/udp_network_test.c
?? build/
```

- Recommended fix: either commit the intended `userland` changes in the `thunderos-userland` repository and update the superproject gitlink, or reset the submodule to the intended committed revision. Then commit the superproject changes, rerun CI, and only merge from a clean tree.
- Test gap: not a code-coverage gap; this is release hygiene and reproducibility.
- Status update: open. Reviewed code surfaces pass Docker/QEMU 10.1.2 verification, but the branch is not mechanically merge-ready until the repository state is normalized.

### Serious

#### S1
- Location: `kernel/fs/ext2_vfs.c`
- Confidence: confirmed
- Problem: `ext2_vfs_write()` treats any non-negative result from `ext2_write_file()` as success and proceeds to write back the inode and clear errno, even if fewer bytes were written than requested.
- Why it matters: for a public-facing OS project, silent partial writes without an explicit contract are data-loss territory. The VFS layer should either guarantee full writes for this call shape or clearly propagate partial-write semantics.
- Evidence:

```c
int bytes_written = ext2_write_file(ext2_fs, inode, offset, buffer, size);
if (bytes_written < 0) {
	return -1;
}
...
clear_errno();
return bytes_written;
```

- Recommended fix: if this API is meant to behave like a full write, reject `bytes_written != size` and preserve the failure. If partial writes are allowed, document that clearly and ensure upper layers handle them correctly.
- Test gap: no visible test asserts partial-write behavior through the VFS ext2 path.
- Status update: fixed in code. `ext2_vfs_write()` now rejects short writes by treating `bytes_written != size` as `THUNDEROS_EIO` instead of reporting success, and the ext2 VFS unit suite now includes a regression that forces a shortened lower-level write request and asserts the failure path. Verified by building with `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a supported Docker/QEMU 10.1.2 test-mode boot.

#### S2
- Location: `tests/unit/`
- Confidence: confirmed
- Problem: the unit test set includes files such as `test_pipes.c`, `test_errno.c`, `test_ext2_vfs.c`, `test_memory_mgmt.c`, and `test_syscalls.c`, but no dedicated tests for `rwlock`, `mutex`, `condvar`, or `wait_queue` behavior.
- Why it matters: the highest-risk confirmed bugs in this audit are synchronization bugs, and the current unit suite is not aimed at them. That makes regressions likely and detection late.
- Recommended fix: add focused synchronization tests for contention, wakeup ordering, lost-wakeup prevention, and failure paths such as allocation failure while sleeping.
- Test gap: this finding is itself the test gap.
- Status update: fixed in code. Added `tests/unit/test_sync_primitives.c` with mutex, rwlock, and condition-variable coverage for trylock behavior, writer-priority blocking, and wake-queue draining paths, alongside the existing wait-queue failure regression. Verified by building with `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a supported Docker/QEMU 10.1.2 test-mode boot.

#### S3
- Location: `README.md`
- Confidence: confirmed
- Problem: the top-level public-facing documentation describes status and features but does not clearly set known constraints or release boundaries for prospective users.
- Why it matters: a public release without explicit limitations invites the wrong expectations. For an educational RISC-V OS, users should not have to infer support boundaries from code or roadmap language.
- Recommended fix: add a “Known Limitations” or “Current Constraints” section covering scope and operational limits before release.
- Test gap: documentation accuracy review should be part of release verification.
- Status update: fixed in docs. `README.md` now includes a release-facing “Known Limitations” section that sets expectations around platform scope, filesystem support, current hardening level, and interface stability.

#### S4
- Location: `kernel/drivers/fbconsole.c`
- Confidence: confirmed
- Problem: `fbcon_scroll_up()` subtracts `FONT_HEIGHT` from `console_height` in unsigned arithmetic, then uses the result for both the copy loop bound and the last-row clear offset.
- Why it matters: if the framebuffer is shorter than one glyph row, `console_height - FONT_HEIGHT` wraps to a very large value and the console scroll path writes far past the framebuffer.
- Evidence:

```c
uint32_t console_height = g_fbcon.rows * FONT_HEIGHT;
for (uint32_t y = 0; y < console_height - FONT_HEIGHT; y++) {
	...
}
uint32_t last_row_start = (console_height - FONT_HEIGHT) * info.width;
```

- Recommended fix: if `g_fbcon.rows == 0` or `console_height < FONT_HEIGHT`, return early or fall back to a plain clear. Do not subtract `FONT_HEIGHT` until the bounds are proven safe.
- Test gap: no framebuffer-console tests exercise tiny or malformed framebuffer geometries.
- Status update: fixed in code. `fbcon_scroll_up()` now bails out cleanly on zero-row and short-framebuffer geometries and uses `size_t`-based offsets only after the bounds are proven safe. Verified by `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a full `make test` run inside Docker/QEMU 10.1.2.

#### S5
- Location: `kernel/drivers/framebuffer.c`, `kernel/drivers/virtio_gpu.c`
- Confidence: confirmed
- Problem: framebuffer dimensions are multiplied and added without overflow checks when computing allocation sizes, clear counts, and pixel indices.
- Why it matters: a malformed or oversized display geometry can turn these calculations into short allocations or wrapped indices, which then produce out-of-bounds writes in the kernel framebuffer path.
- Evidence:

```c
g_gpu_device->fb_size = (size_t)g_gpu_device->fb_width * g_gpu_device->fb_height * 4;
g_gpu_device->fb_pixels[y * g_gpu_device->fb_width + x] = bgrx;
uint32_t num_pixels = g_gpu_device->fb_width * g_gpu_device->fb_height;
uint32_t num_pixels = g_fb_info.width * g_fb_info.height;
```

- Recommended fix: add checked helpers for multiplication and index arithmetic, reject impossible geometries before allocation, and guard region-clamp expressions such as `x + width` and `y + height` against wraparound before comparing them to framebuffer bounds.
- Test gap: there are no driver tests covering malicious or overflow-inducing framebuffer dimensions.
- Status update: fixed in code. The framebuffer path now validates dimensions before deriving pitch or clear counts, and the VirtIO GPU path now uses checked helpers for framebuffer size, pixel count, pixel index, and region clamp arithmetic. Verified by `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a full `make test` run inside Docker/QEMU 10.1.2.

#### S6
- Location: `kernel/drivers/virtio_blk.c`
- Confidence: confirmed
- Problem: both read and write paths validate capacity with `if (sector + count > g_blk_device->capacity)`.
- Why it matters: `sector` is `uint64_t` and `count` is `uint32_t`; a near-`UINT64_MAX` request can wrap the sum and bypass the bounds check, allowing out-of-range I/O against the block device.
- Evidence:

```c
if (sector + count > g_blk_device->capacity) {
	RETURN_ERRNO(THUNDEROS_EINVAL);
}
```

- Recommended fix: first reject `sector > capacity`, then validate `count > capacity - sector` so the bounds check cannot overflow.
- Test gap: no block-device tests cover extreme sector values or overflow-style bounds checks.
- Status update: fixed in code. Both block-device I/O entry points now validate bounds through a helper that rejects `sector > capacity` and checks the remaining span without allowing arithmetic wraparound. Verified by `make BUILD_DIR=build-user ENABLE_TESTS=1 TEST_MODE=1 all` and by a full `make test` run inside Docker/QEMU 10.1.2.

#### S7
- Location: `userland/core/chown.c`, `userland/core/sleep.c`
- Confidence: confirmed
- Problem: userland numeric parsing and time conversion paths silently overflow instead of rejecting out-of-range input.
- Why it matters: `chown` can wrap large numeric IDs into the wrong `uid`/`gid`, and `sleep` can wrap `seconds * 1000` into an incorrect interval.
- Evidence:

```c
while (isdigit(*str)) {
	result = result * 10 + (*str - '0');
	str++;
}
```

```c
long seconds = atol(argv[1]);
syscall(SYS_SLEEP, seconds * 1000, 0, 0);
```

- Recommended fix: reject numeric input once `result > (limit - digit) / 10`, and reject `seconds > LONG_MAX / 1000` before converting to milliseconds.
- Test gap: there are no userland argument-validation tests for numeric overflow boundaries.
- Status update: fixed in code. `chown` now rejects overflowing numeric owner/group input during parse, and `sleep` now parses seconds with explicit overflow checks before the milliseconds conversion. Verified by `./build_userland.sh` and by the Docker/QEMU `make test` userland build path.

#### S8
- Location: `userland/core/mkdir.c`, `userland/core/rmdir.c`
- Confidence: confirmed
- Problem: both binaries are still placeholders. They print a usage note and exit without calling `SYS_MKDIR` or `SYS_RMDIR`.
- Why it matters: the public docs and build scripts present these as core utilities, but the shipped programs do not implement the advertised functionality.
- Evidence:

```c
print("mkdir: usage: mkdir <directory>\n");
print("Note: mkdir requires argument passing from shell (not yet implemented)\n");
syscall(SYS_EXIT, 0, 0, 0);
```

- Recommended fix: implement real argument handling and syscall invocation, or stop building and documenting these binaries until they are functional.
- Test gap: there is no userland smoke test that validates the built core utilities against their documented behavior.
- Status update: fixed in code. `mkdir` and `rmdir` now accept `argc`/`argv`, report missing operands, and call the actual directory syscalls instead of exiting as placeholders. Verified by `./build_userland.sh` and by the Docker/QEMU `make test` userland build path.

### Minor

#### M1
- Location: repository root
- Confidence: confirmed
- Problem: there is no `SECURITY.md` describing how to report vulnerabilities or what support/disclosure expectations exist.
- Why it matters: once the repo is public, this becomes basic project hygiene.
- Recommended fix: add `SECURITY.md` with a reporting contact/path, disclosure expectations, and version support guidance.
- Test gap: not applicable.
- Status update: fixed in docs. Added `SECURITY.md` with scope, supported-version guidance, private reporting expectations, and disclosure coordination guidance.

#### M2
- Location: `.github/workflows/ci.yml`
- Confidence: confirmed from workflow structure and repository conventions
- Problem: current CI emphasis is pass/fail, but there is no release-facing log retention strategy documented in the repo for failed test diagnosis.
- Why it matters: once external contributors begin using CI, diagnosis speed matters.
- Recommended fix: upload test and static-analysis logs as artifacts on failure.
- Test gap: not applicable.
- Status update: fixed in CI. `.github/workflows/ci.yml` now preserves build, QEMU boot, test-suite, and warning logs under `ci-logs/` and uploads them as workflow artifacts with `if-no-files-found: warn`.

#### M3
- Location: `userland/net/udp_network_test.c`
- Confidence: confirmed
- Problem: the network test hard-codes stale socket syscall numbers (`66`-`69`) instead of the current kernel values (`100`-`103`), and its `recvfrom` call passes a null address-length argument.
- Why it matters: even if the kernel network path works, this test program cannot reliably exercise it and may invoke the wrong syscalls entirely.
- Evidence:

```c
#define SYS_SOCKET   66
#define SYS_BIND     67
#define SYS_SENDTO   68
#define SYS_RECVFROM 69
...
int rcvd = syscall6(SYS_RECVFROM, sock, (long)recv_buf, sizeof(recv_buf), 0, (long)&from, 0);
```

- Recommended fix: include or mirror the current syscall constants from `include/kernel/syscall.h`, and pass an explicit address-length pointer for `recvfrom`.
- Test gap: the network test is not part of the default userland build, so this drift currently has no automated coverage.
- Status update: fixed in code. `udp_network_test.c` now uses the current socket syscall numbers and passes an explicit receive-address-length pointer to `SYS_RECVFROM`. Verified by `./build_userland.sh`.

### Serious (post-initial-audit)

#### S9
- Location: `userland/bin/ush.c` (`execute_pipeline`), `kernel/fs/vfs.c` (`g_file_table`)
- Confidence: confirmed
- Problem: The shell's `execute_pipeline()` used fork+exec+dup2 to connect two commands through a kernel pipe. This stalled indefinitely when run inside a `source` script. Three interacting design issues combined to cause the hang:
  1. **Global FD table**: `kernel/fs/vfs.c` uses a single `static vfs_file_t g_file_table[VFS_MAX_OPEN_FILES]` shared by all processes. `dup2(pipe_write, STDOUT_FD)` in a child process overwrites `g_file_table[1]` globally, corrupting the entry for every process.
  2. **Hardcoded FDs 0-2**: `sys_read(0)` always reads from vterm and `sys_write(1)` always writes to vterm, completely bypassing the file table. So even after `dup2`, the exec'd program's reads/writes still go to vterm, not to the pipe.
  3. **No FD cleanup on exit**: `process_exit()` marks the process as zombie but never closes its file descriptors. The pipe write end's reference count (`pipe->write_ref_count`) never reaches zero, so the reader never gets EOF and blocks in `wait_queue_sleep()` forever.
- Why it matters: The pipeline is a core shell feature advertised in help text and tested in the soak suite. The hang caused 4/12 soak test failures and required QEMU to be killed by timeout.
- Evidence: `cat /pipe.txt | cat` inside `source /soak_script.sh` produced correct output ("Pipe path works") but the right-side `cat` then blocked forever waiting for EOF that never came. The shell never returned to process remaining script lines.
- Root cause chain: fork right child → `dup2(pipefd[0], STDIN_FD)` → `execve("/bin/cat")` → `cat_fd(STDIN_FD)` → `sys_read(0)` → kernel enters vterm busy-wait loop (ignoring pipe entry in file table) → child never reads from pipe → left child exits but pipe write refcount stays at 1 (g_file_table[1] still references it) → right child never gets EOF → parent `sys_wait(right_pid)` blocks forever.
- Fix applied: Replaced `execute_pipeline()` with an in-process implementation that runs builtin commands directly, reading/writing through pipe FDs (≥ 3) that work correctly with `vfs_read`/`vfs_write`, avoiding fork, exec, and dup2 on the reserved FDs 0-2 entirely. The kernel pipe lifecycle (create → write → close write end → read until EOF → close read end) now works correctly.
- Architectural note: The global FD table is a known design limitation that prevents correct POSIX-style fork+exec+dup2 pipelines. Per-process FD tables would be the proper fix but require significant kernel refactoring and are tracked as a roadmap item, not a release blocker.
- Test gap: The shell soak test now covers pipeline execution inside `source` scripts. 12/12 soak tests pass.
- Status update: fixed in code. `execute_pipeline()` was rewritten as an in-process builtin pipeline. Historical shell-soak verification existed during an earlier audit pass; the current authoritative verification for this branch state is the Docker/QEMU 10.1.2 30/30 run recorded in the executive summary and final re-audit.

#### M4
- Location: `tests/scripts/structured_test_helpers.sh`
- Confidence: confirmed
- Problem: `testfmt_run()` uses `grep -q "$pat"` (basic regex) but test patterns like `"System Poweroff Requested|Initiating system shutdown"` use `|` for alternation, which requires extended regex (`-E` flag). The `|` was treated as a literal character, causing the pattern to never match even though the text was present in the output.
- Why it matters: The shutdown assertion failed despite the expected text being clearly visible in the last 8 lines of output, giving a false failure on an otherwise passing test.
- Fix applied: Changed `grep -q` to `grep -qE` in `testfmt_run()` and `grep -qi` to `grep -qEi` in `testfmt_run_absent()`.
- Status update: fixed. Verified by Docker/QEMU soak run with 12/12 pass.

## Coverage Matrix
| Surface | Files / Modules | Status | Notes |
|---------|------------------|--------|-------|
| Boot path | `boot/`, `kernel/main.c`, `kernel/arch/riscv64/` | partially reviewed | No blocker found in first pass, but not audited exhaustively end-to-end |
| Memory management | `kernel/mm/`, `include/mm/` | reviewed | Page-based kmalloc, PMM contiguous allocation, and allocator failure paths reviewed; limitations are documented but no release-blocking defect remains confirmed |
| Filesystem and VFS | `kernel/fs/`, `include/fs/` | partially reviewed | Confirmed ext2 VFS write semantics issue; broader VFS audit beyond the exercised release paths is still sampled rather than exhaustive |
| Processes and syscalls | `kernel/core/`, `include/kernel/` | reviewed | Shell argv handoff fixed; scheduler fairness and shell runtime behavior reviewed as residual-risk areas rather than open defects |
| Drivers and VirtIO | `kernel/drivers/`, `include/drivers/` | reviewed | Confirmed framebuffer console underflow, framebuffer/GPU arithmetic overflow risks, and VirtIO block capacity-check overflow |
| Userland | `userland/` | reviewed | Confirmed shell-to-loader argv loss, numeric overflow in argument-driven utilities, placeholder mkdir/rmdir binaries, stale network test syscall wiring, and pipeline stall due to global FD table interaction |
| Tests | `tests/`, `userland/tests/` | reviewed | Wait-queue, mutex, rwlock, condvar, ext2 partial-write regression coverage added in the kernel test build; shell soak test suite added with 12 end-to-end assertions |
| Build and CI | `Makefile`, scripts, `.github/workflows/` | reviewed | Supported Docker/QEMU 10.1.2 runtime verification completed successfully after rebuilding the pinned Docker image and correcting test-runner log capture |
| Documentation and release assets | `README.md`, `CHANGELOG.md`, `ROADMAP.md`, `docs/` | reviewed | README now includes limitations and explicit current system limits; roadmap remains directional but not release-blocking |
| Repository hygiene | `.gitignore`, `LICENSE`, `CONTRIBUTING.md`, `AI_USAGE.md`, `SECURITY.md` | reviewed | Security reporting path and audit workflow guidance are in place |

## Residual Risks And Limits
- Userland runtime behavior under heavier interactive use is now covered by a 12-test soak suite exercising source scripts, file I/O, redirections, environment variables, pipelines, history, job control, and graceful shutdown. The shell is intentionally synchronous and foreground-oriented.
- Scheduler fairness was reviewed and no starvation defect was confirmed, but queue removal remains `O(n)` and has not been stress-tested near `MAX_PROCS` under pathological fork/exit churn.
- Memory management was reviewed and no corruption issue was confirmed, but `kmalloc` remains page-based and the PMM contiguous search remains linear, so fragmentation/performance limits should be treated as known design constraints rather than bugs.
- Native host verification on this workstation remains non-authoritative because the installed host QEMU is `6.2.0`; the supported verification path is the freshly rebuilt Docker image pinned to QEMU `10.1.2`.

## Merge / Release Recommendation
- Merge now / Do not merge: Do not merge yet
- Release now / Do not release: Do not release yet
- Preconditions:
	- Commit or reset the `userland/` submodule so it points at a real committed revision with no local dirt
	- Commit the superproject changes, including the intended `userland` gitlink and Docker-backed run path, from a clean working tree
	- Rerun CI on the cleaned branch and keep Docker/QEMU 10.1.2 as the authoritative verification path

## Follow-Up Recommendations
1. ~~Add targeted scheduler and shell soak tests so fairness and long-running interactive behavior move from sampled review to repeatable evidence.~~ Done: shell soak test suite added with 12 end-to-end assertions; scheduler soak test added to kernel unit suite.
2. Expand MM stress coverage around allocator fragmentation and large contiguous allocations.
3. Upgrade the host workstation QEMU to `10.1.2+` if native local verification is required alongside the Docker path.
4. Implement per-process file descriptor tables to replace the global `g_file_table` in `kernel/fs/vfs.c`. This is the architectural prerequisite for correct POSIX-style fork+exec+dup2 pipelines and proper FD cleanup on process exit. The current in-process pipeline workaround in `ush.c` is correct for builtin commands but does not extend to arbitrary external command pipelines.

---

# Re-Audit: Final Merge Review (Silvanus Trold)

**Date**: 2026-03-17  
**Branch**: `fix/code-review-v0.9.0` → `main`  
**Reviewer**: Silvanus Trold (ruthless senior systems programmer persona)  
**Protocol**: `exhaustive-review.md` + `production-ready-check.md` from `ai-dev-prompts/`  
**Scope**: Current branch plus working-tree deltas intended for the merge PR, with emphasis on the recently fixed kernel/runtime surfaces, submodule migration, CI, and supported run paths.

---

## Verification Summary

| Check | Result |
|-------|--------|
| Clean build with `-Werror`, `ENABLE_TESTS=1`, `TEST_MODE=1` | **PASS** |
| Current available test suite: 30/30 (25 kernel + 5 integration) | **PASS** |
| Docker/QEMU 10.1.2 test path | **PASS** |
| `./run_os_docker.sh` boot to shell prompt | **PASS** |
| Host `./run_os.sh` on QEMU 6.2.0 | **PASS** (fails fast with explicit unsupported-version error) |
| Waitpid stopped-child `EFAULT` regression test | **PASS** |
| Dirty working tree / dirty submodule | **FAIL** |

---

## Merge Verdict

**Not mergeable yet.**

The code fixes are fine. The Docker-backed run path is fine. The host-side QEMU guard is fine. The branch is still not fit to merge for one boring reason: the repository state is messy. You do not merge a superproject that points at a dirty submodule and still has local-only files floating around.

When the tree is clean and the submodule points at a committed revision, this looks mergeable on the reviewed surfaces.

---

## Current Findings

### Open blocker

#### B4 — dirty superproject and dirty `userland/` submodule
- **Location**: repository root, `userland/`
- **Severity**: Blocker
- **Problem**: the superproject still has broad staged/unstaged changes, `run_os_docker.sh` is still local-only, and the `userland/` submodule has local modifications plus generated build output.
- **Why it blocks merge**: reviewers and CI cannot reproduce whatever happens to be in your filesystem today. The superproject must point at a real submodule commit, and the submodule itself must be either committed or reset.
- **Required fix**:
	1. Commit or reset the `userland/` changes inside `thunderos-userland`.
	2. Update the superproject gitlink to the intended `userland` commit.
	3. Commit the remaining superproject changes, including the Docker run helper.
	4. Rerun CI on the cleaned branch.

### No remaining open blocker/serious code findings on the reviewed surfaces

The previously reviewed kernel/runtime issues are in good shape now:
- `sys_waitpid()` validates stopped-child `wstatus` before writing through it.
- Timed sleeper wakeups now use the process-table lock discipline instead of mutating state unsynchronized from the timer interrupt path.
- QEMU 10.1.2 is now enforced on the host path, and there is a supported Docker-backed run script for everyone else.
- CI now checks out submodules recursively, and the superproject declares the userland dependency explicitly.

Residual architectural limits still exist, but they are not fresh blockers for this merge:
- global FD table and in-process pipeline workaround
- single-CPU assumptions in parts of filesystem and block-device code
- long process/exec code paths that are correct but still ugly

---

## Merge / Release Recommendation (Current)

**Merge: NO** — not until B4 is cleared.  
**Release: NO** — not from a dirty tree.  

**Preconditions**:
1. Normalize and commit the `userland/` submodule state.
2. Commit the superproject state, including `run_os_docker.sh` and the QEMU 10.1.2 guard changes.
3. Re-run CI on the pushed branch.
4. Merge only from a clean working tree.

Once that is done: fine.