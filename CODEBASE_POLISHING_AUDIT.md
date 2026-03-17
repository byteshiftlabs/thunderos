# ThunderOS Codebase Polishing Audit

## Scope

This file is the authoritative audit ledger for branch `code-refactor/codebase-polishing`.

It follows the `ai-dev-prompts` workflow:
- map review surfaces first
- record only confirmed findings as blockers, serious issues, or minor issues
- keep a coverage matrix so no subsystem is silently skipped
- fix in severity order and update this ledger in place

Current audit status: initial inventory complete, full subsystem review not yet complete.

## Executive Summary

- Total blockers: 0 confirmed
- Total serious issues: 0 open
- Total minor issues: 0 confirmed
- Overall recommendation: do not treat this branch as release-ready until the ordered audit below is completed.

## Findings Table

| ID | Severity | Confidence | File | Issue | Recommended Fix |
|----|----------|------------|------|-------|-----------------|
| S1 | Serious | Confirmed | README.md; tests/README.md; docs/source/conf.py; docs/source/api.rst; VERSION | Public release metadata was inconsistent across repo surfaces | Fixed by introducing a canonical `VERSION` file and aligning release-facing docs to `0.9.0` |
| S2 | Serious | Confirmed | kernel/fs/vfs.c; kernel/core/syscall.c; kernel/core/shell.c; include/fs/vfs.h | VFS path resolution leaked dynamically allocated nodes and path normalization could silently truncate oversized paths while leaving stale errno on success | Fixed by adding explicit node release, freeing transient lookup nodes during traversal and non-open operations, and returning `THUNDEROS_ERANGE` on oversized normalized paths |
| S3 | Serious | Confirmed | kernel/mm/dma.c; kernel/mm/kmalloc.c | Allocator APIs could fail on invalid or overflowed requests without setting `errno`, and DMA allocation did not clear `errno` on success | Fixed by rejecting zero-size and overflowed requests explicitly and normalizing allocator errno behavior |
| S4 | Serious | Confirmed | tests/scripts/test_kernel.sh; tests/scripts/test_integration.sh | Standalone scripted QEMU tests built `TEST_MODE=1` kernels without `ENABLE_TESTS=1`, so the built-in test markers they asserted could never appear | Fixed by aligning the standalone scripts with the same build mode used by `make test` |
| S5 | Serious | Confirmed | kernel/fs/ext2_vfs.c; kernel/fs/vfs.c | Lookup wrappers overwrote specific ext2/VFS failures with generic `ENOENT`, hiding real causes like invalid directories, allocation failures, or mount-state errors | Fixed by preserving upstream `errno` and only defaulting to `ENOENT` when no specific error is set |
| S6 | Serious | Confirmed | kernel/mm/paging.c | User page-table and mapping failure paths leaked paging structures or left dangling mappings to freed pages | Fixed by freeing page tables with `free_page_table()` and cleaning up partially mapped user pages on failure |
| S7 | Serious | Confirmed | kernel/core/process.c; kernel/core/syscall.c | User-memory teardown leaked physical pages on process cleanup and `munmap`, and kernel-mode process stacks were not fully released | Fixed by freeing VMA-backed physical pages during cleanup and unmapping, and by freeing kmalloc-backed kernel-process user stacks |
| S8 | Serious | Confirmed | kernel/core/syscall.c | Several syscall wrappers returned bare `SYSCALL_ERROR` on validation failures, leaving user-visible `errno` stale or unset | Fixed by centralizing syscall path validation and assigning deterministic errno values for common bad-pointer, bad-fd, invalid-argument, and no-such-process cases |
| S9 | Serious | Confirmed | kernel/core/syscall.c | Several remaining syscall wrappers still misreported invalid user buffers as `EINVAL`, skipped shared path validation, or left stale `errno` on success-only getters | Fixed by normalizing the remaining pointer-validation sites to `EFAULT`, reusing shared path validation, and clearing errno on successful identity and TTY getters |

## Detailed Findings

### Blockers

No confirmed blockers recorded yet.

### Serious

#### S1
- Location: `README.md`, `tests/README.md`, `docs/source/conf.py`, `docs/source/api.rst`, `VERSION`
- Confidence: confirmed
- Problem: the repository exposed conflicting release metadata. `README.md` presented ThunderOS as version `0.9.0`, `tests/README.md` said `v0.7.0+`, `docs/source/conf.py` set the Sphinx release to `0.1.0`, and `docs/source/api.rst` still described the public API as `v0.4.0`.
- Why it matters: this is public-facing drift across the main landing page, test docs, and generated documentation. It undermines release clarity and makes downstream verification harder because users cannot tell which version the docs actually describe.
- Recommended fix: establish a single canonical version for this branch and update all public-facing docs and generated-doc metadata to match. This branch now uses a top-level `VERSION` file as that source for Sphinx metadata, and the known mismatched entry points have been aligned to `0.9.0`.
- Test gap: no automated check currently validates version consistency across release-facing files.
- Verification note: added `VERSION`, updated `tests/README.md` to `v0.9.0+`, updated `docs/source/conf.py` to read the canonical version, and updated `docs/source/api.rst` to `v0.9.0`.
- Status: fixed

#### S2
- Location: `kernel/fs/vfs.c`, `kernel/core/syscall.c`, `kernel/core/shell.c`, `include/fs/vfs.h`
- Confidence: confirmed
- Problem: `vfs_resolve_path()` walked ext2 directories by allocating new `vfs_node_t` and inode objects for each component, but it did not release intermediate nodes while traversing. Several non-open callers also kept the final resolved node alive after one-shot operations like `stat`, `exists`, `chdir`, and shell `ls`. In the same area, `vfs_normalize_path()` silently truncated oversized paths/components and did not clear `errno` on success.
- Why it matters: this combines a real memory leak in common filesystem paths with incorrect path semantics. Repeated path-heavy operations can steadily leak heap memory, and silent truncation can resolve the wrong path while preserving stale `errno` from earlier failures.
- Recommended fix: add an explicit node-release helper, use it consistently for transient lookup nodes and non-FD callers, and make path normalization fail with `THUNDEROS_ERANGE` when the normalized path cannot be represented safely.
- Test gap: there is no regression coverage for repeated path resolution, directory listing, or oversize path normalization behavior.
- Verification note: added `vfs_release_node()`, released transient nodes during path traversal and one-shot callers, and changed normalization helpers to return `THUNDEROS_ERANGE` instead of truncating.
- Status: fixed

#### S3
- Location: `kernel/mm/dma.c`, `kernel/mm/kmalloc.c`
- Confidence: confirmed
- Problem: `dma_alloc()` returned `NULL` for zero-size requests and PMM allocation failures without setting `errno`, and `kmalloc()` returned `NULL` for zero-size requests without setting `errno`. Neither API rejected arithmetic overflow before rounding or header accounting.
- Why it matters: callers can receive indistinguishable `NULL` failures for programmer error, overflow, and genuine exhaustion, which breaks the repo's `errno` contract and makes allocator failures harder to diagnose safely.
- Recommended fix: treat zero-size requests as `THUNDEROS_EINVAL`, reject arithmetic overflow with `THUNDEROS_ERANGE`, set `THUNDEROS_ENOMEM` on allocation failure, and clear `errno` on successful DMA allocation.
- Test gap: there is no direct unit coverage for allocator `errno` behavior on invalid input and overflow edge cases.
- Verification note: updated `kmalloc()` and `dma_alloc()` to set deterministic `errno` on invalid, overflow, and OOM paths, and to clear `errno` on successful DMA allocation.
- Status: fixed

#### S4
- Location: `tests/scripts/test_kernel.sh`, `tests/scripts/test_integration.sh`
- Confidence: confirmed
- Problem: the standalone scripted QEMU tests compiled the kernel with `TEST_MODE=1` only, while the in-kernel memory and ELF tests are compiled conditionally under `ENABLE_KERNEL_TESTS`. The scripts then asserted for markers that could never be emitted by the binary they had built.
- Why it matters: this creates false negatives in the scripted regression suite and weakens trust in test results, especially when developers run the standalone scripts directly instead of `make test`.
- Recommended fix: build the scripted test kernels with `ENABLE_TESTS=1 TEST_MODE=1`, matching the existing `make test` path.
- Test gap: the repo does not currently self-check consistency between `make test` and the standalone scripts.
- Verification note: updated both standalone scripts to use `ENABLE_TESTS=1 TEST_MODE=1`.
- Status: fixed

#### S5
- Location: `kernel/fs/ext2_vfs.c`, `kernel/fs/vfs.c`
- Confidence: confirmed
- Problem: `ext2_vfs_lookup()` converted any `ext2_lookup()` failure into `THUNDEROS_ENOENT`, and higher VFS callers like `vfs_open()`, `vfs_rmdir()`, and `vfs_unlink()` similarly rewrote failed path resolution to generic `ENOENT`. That flattened real upstream failures such as `THUNDEROS_EFS_BADDIR`, `THUNDEROS_ENOMEM`, or `THUNDEROS_EFS_NOTMNT`.
- Why it matters: callers lose the actual reason a lookup failed, making both debugging and user-visible error reporting less accurate. It also violates the repo's requirement to preserve `errno` from callees when they already set a more specific failure.
- Recommended fix: preserve `errno` when lookup/resolution fails and only synthesize `THUNDEROS_ENOENT` if no prior error has been recorded.
- Test gap: there is no negative-path coverage asserting that lookup failures preserve specific `errno` values instead of collapsing to `ENOENT`.
- Verification note: updated ext2/VFS lookup wrappers to only set `THUNDEROS_ENOENT` when `errno` is still zero.
- Status: fixed

#### S6
- Location: `kernel/mm/paging.c`
- Confidence: confirmed
- Problem: `create_user_page_table()` freed page tables with `kfree()` even though they come from `alloc_page_table()`/`pmm_alloc_page()`. Separately, `map_user_code()` and `map_user_memory()` could fail after mapping some user pages, but only freed physical pages, leaving stale page-table entries or partially mapped ranges behind.
- Why it matters: this can corrupt allocator assumptions, leak page-table pages, and leave dangling mappings to freed physical pages in partially initialized user address spaces.
- Recommended fix: use `free_page_table()` for user page-table construction failures, and on user mapping failures, unmap any pages that were already inserted before freeing their physical memory.
- Test gap: there is no failure-injection coverage for partial user mapping cleanup or user page-table construction rollback.
- Verification note: replaced incorrect `kfree(user_pt)` calls with `free_page_table(user_pt)` and added rollback cleanup for partially mapped user pages in both mapping helpers.
- Status: fixed

#### S7
- Location: `kernel/core/process.c`, `kernel/core/syscall.c`
- Confidence: confirmed
- Problem: process teardown only freed VMA structures and page tables, not the physical pages mapped for user code, heap, stack, or `mmap` regions. `sys_munmap()` similarly removed mappings without releasing physical backing pages. Separately, kernel-mode processes allocate `user_stack` with `kmalloc()`, but `process_free()` did not release that allocation.
- Why it matters: exiting user processes and explicit unmaps leaked physical memory, and long-running systems would eventually exhaust pages. The kernel-process stack case also leaks heap memory for every kernel-mode process that exits.
- Recommended fix: release VMA-backed physical pages before freeing page tables or VMAs, free physical pages during `munmap`, and explicitly free kmalloc-backed user stacks for kernel-mode processes.
- Test gap: there is no stress coverage for repeated `mmap`/`munmap`, repeated user-process create/exit cycles, or kernel-process lifetime cleanup.
- Verification note: added VMA page-release cleanup in process teardown and region rollback paths, updated `sys_munmap()` to free physical pages, and freed kmalloc-backed kernel-process user stacks during process cleanup.
- Status: fixed

#### S8
- Location: `kernel/core/syscall.c`
- Confidence: confirmed
- Problem: multiple syscall wrappers validated pointers, path strings, file descriptors, process state, or arguments and then returned `SYSCALL_ERROR` directly without first setting `errno`. That left userland observing stale `errno` from unrelated earlier failures or no specific failure code at all.
- Why it matters: this breaks the syscall contract at the user/kernel boundary. User programs can see misleading errors from wrappers like `open`, `stat`, `read`, `write`, `waitpid`, `kill`, `mmap`, `munmap`, and terminal-management syscalls even when the kernel already knows the precise validation failure.
- Recommended fix: centralize common path-argument validation, and assign deterministic `errno` values for common wrapper failures such as invalid user pointers, oversized path arguments, invalid file descriptors, invalid process state, and missing child or target processes.
- Test gap: there is no direct syscall regression coverage asserting `errno` values for user-pointer, bad-fd, and bad-process negative paths.
- Verification note: added a shared syscall path validator and updated the affected wrappers to set `THUNDEROS_EFAULT`, `THUNDEROS_ERANGE`, `THUNDEROS_EBADF`, `THUNDEROS_EINVAL`, `THUNDEROS_ECHILD`, `THUNDEROS_ESRCH`, `THUNDEROS_ENOMEM`, or `THUNDEROS_EIO` before returning `SYSCALL_ERROR` on these boundary checks.
- Status: fixed

#### S9
- Location: `kernel/core/syscall.c`
- Confidence: confirmed
- Problem: after the first syscall errno pass, some wrappers still had inconsistent boundary behavior. `getdents()` and `getcwd()` treated invalid user buffers as `THUNDEROS_EINVAL` instead of `THUNDEROS_EFAULT`, `chdir()` and `execve()` bypassed the shared path validator, `execve()` could return a bare error for bad argument pointers, and simple getter syscalls like `getuid()` and `gettty()` could return success with stale errno still set.
- Why it matters: these inconsistencies keep the user/kernel contract unpredictable. Callers can still receive the wrong class of error for bad user memory, and successful metadata queries can inherit unrelated earlier failures.
- Recommended fix: normalize the remaining invalid-pointer sites to `THUNDEROS_EFAULT`, route path-taking wrappers through the shared validator, reject oversized `execve()` argument vectors deterministically, and clear errno on simple success-only getters.
- Test gap: there is still no dedicated syscall regression suite in the tracked kernel tests for bad-fd, tty, and `waitpid` negative paths.
- Verification note: updated `getdents()`, `getcwd()`, `chdir()`, `execve()`, and the simple identity and terminal getters so their errno behavior matches the rest of the audited syscall boundary.
- Status: fixed

### Minor

No confirmed minor issues recorded yet.

## Coverage Matrix

| Surface | Files / Modules | Status | Notes |
|---------|------------------|--------|-------|
| Build and release metadata | Makefile, build scripts, Dockerfile, CI workflows, release docs | Partially reviewed | Initial inventory complete; version drift confirmed in public docs; runtime and reproducibility gates still need review |
| Public documentation | README.md, CHANGELOG.md, ROADMAP.md, docs/, CONTRIBUTING.md, AI_USAGE.md | Partially reviewed | Initial pass done on major entry points; known release-metadata drift fixed; broader accuracy sweep still pending |
| Test harness and CI | tests/, GitHub workflows, static analysis scripts | Partially reviewed | Structure and current commands reviewed; coverage quality and missing cases still need audit |
| Kernel error handling | include/kernel/errno.h, kernel/core/, kernel/fs/, kernel/drivers/ | Not reviewed | High-priority surface because ThunderOS depends on consistent errno propagation |
| Memory management | kernel/mm/, include/mm/ | Not reviewed | Audit should cover allocation failures, cleanup paths, DMA usage, and page-table correctness |
| VirtIO block driver | kernel/drivers/virtio_blk.c, include/drivers/virtio_blk.h | Not reviewed | High-risk subsystem for real I/O correctness and boot-time filesystem access |
| Filesystem and VFS | kernel/fs/, include/fs/ | Not reviewed | Audit should cover ext2 integrity, bounds checks, path handling, and errno behavior |
| Process, scheduler, signals | kernel/core/process.c, scheduler.c, signal.c, wait_queue.c | Not reviewed | Audit should cover lifecycle, wakeups, exit/wait semantics, and state transitions |
| Syscalls and trap path | kernel/core/syscall.c, arch trap/interrupt code, user return code | Not reviewed | Audit should cover user/kernel boundary validation and recovery behavior |
| Synchronization and IPC | mutex, rwlock, condvar, pipe code | Not reviewed | Audit should cover race conditions, blocking semantics, and cleanup |
| ELF loading and userland execution | kernel/core/elf_loader.c, userland/, build_userland.sh | Not reviewed | Audit should cover loader validation, argument setup, userland packaging, and exec contract |
| Device and display stack | framebuffer, fbconsole, vterm, virtio_gpu | Not reviewed | Lower priority than storage/process correctness but still part of release surface |

## Unreviewed Or Uncertain Areas

- Surface: kernel subsystem correctness
- Why unreviewed or uncertain: this ledger is intentionally seeded from inventory plus a narrow documentation verification pass; most runtime-critical subsystems have not yet been audited on this clean branch.
- Required follow-up: review subsystem-by-subsystem in the ordered plan below and log findings here immediately.

- Surface: release reproducibility on a fresh environment
- Why unreviewed or uncertain: CI and scripts were inspected, but the full clean-environment reproduction pass has not been executed as part of this new branch workflow.
- Required follow-up: run the build, test, docs, and QEMU verification gates after the first round of high-severity fixes.

## Merge / Release Recommendation

- Merge now: no
- Release now: no
- Preconditions:
   - resolve any additional blocker or serious findings discovered in the subsystem audit
  - complete the coverage matrix so all major surfaces are reviewed or explicitly marked uncertain with rationale
  - pass the verification gates listed below after fixes

## Ordered Fix Plan

1. Normalize release metadata and documentation entry points.
   - Completed: added `VERSION` as the canonical release marker and aligned the known mismatched entry points.
   - Follow-up: use the documentation sweep to catch any remaining stale version references outside the main release surfaces.

2. Audit errno and error-propagation behavior.
   - Review all failure-returning kernel paths for missing `errno` assignment, incorrect clearing on success, and overwritten upstream errors.
   - Prioritize VFS, ext2, process creation, and syscall entry points.

3. Audit memory-management correctness.
   - Review `kmalloc`, `kfree`, DMA allocation, paging helpers, and cleanup paths.
   - Classify leaks, double-frees, NULL handling failures, and bad physical-address usage.

4. Audit VirtIO block and boot-path I/O.
   - Validate DMA usage, descriptor setup, barriers, interrupt wiring, timeouts, and behavior under the supported QEMU configuration.

5. Audit ext2 and VFS correctness.
   - Validate inode and block bounds checks, path traversal, file descriptor validation, short-read behavior, and mounted-filesystem assumptions.

6. Audit process, scheduler, and signal semantics.
   - Validate fork/exec/wait/exit interactions, sleep/wakeup behavior, foreground process handling, and process-state transitions.

7. Audit syscall and trap boundaries.
   - Validate pointer checks, user-memory access, register preservation, trap recovery, and permission enforcement.

8. Audit synchronization primitives and pipes.
   - Validate blocking semantics, wakeup correctness, fairness expectations, and shutdown/error behavior.

9. Audit ELF loader and userland packaging.
   - Validate ELF header checks, segment mapping, argument/environment setup, and consistency between built binaries and filesystem image population.

10. Expand test coverage where the audit finds gaps.
   - Add focused regression tests for each confirmed blocker or serious issue.
   - Add negative-path tests for errno propagation, ext2 validation, VirtIO timeouts, and process lifecycle edge cases as needed.

11. Audit CI, static analysis, and reproducibility.
   - Check whether current workflows cover the actual high-risk paths.
   - Tighten or add checks only after the underlying behavior is stable.

12. Finish with documentation and release-surface cleanup.
   - Reconcile README, Sphinx docs, test docs, and roadmap language with the verified state of the branch.
   - Update this ledger with final release and merge recommendations.

## Verification Gates

Use these checks after each meaningful fix batch, with targeted runs earlier and the full set before merge:

```bash
make clean && make
make test
./tests/scripts/run_all_tests.sh
cd docs && make html
./tests/static_analysis/run_clang_tidy.sh
```

For runtime-sensitive fixes, also run the supported QEMU boot path and capture enough serial output to verify boot, storage, filesystem mount, and shell/userland behavior.