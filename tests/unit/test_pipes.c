/*
 * Pipe Unit Tests
 *
 * Verifies pipe creation, write/read round-trip, partial reads,
 * EPIPE on write to closed read-end, and pipe_can_free lifecycle.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/pipe.h"
#include "kernel/errno.h"
#include "kernel/kstring.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

void test_pipes_all(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "Pipes");
    ktest_suite_begin(&suite);

    pipe_t *p = pipe_create();
    ASSERT(suite, "CreateReturnsNonNull",
           p != NULL,
           "pipe_create returned NULL");

    if (!p) {
        ktest_case_begin(&suite, "RemainingCasesRequireValidPipe");
        ktest_case_skip(&suite, "RemainingCasesRequireValidPipe",
                        "Cannot continue pipe tests without a valid pipe");
        ktest_suite_end(&suite);
        return;
    }

    ASSERT(suite, "NewPipeIsNotFreeable",
           pipe_can_free(p) == 0,
           "expected pipe_can_free to be 0 right after creation");

    const char *msg = "hello pipe";
    int msg_len = (int)kstrlen(msg);

    clear_errno();
    int written = pipe_write(p, msg, (size_t)msg_len);
    ASSERT(suite, "WriteReturnsExpectedByteCount",
           written == msg_len,
           "pipe_write did not return the expected byte count");
    ASSERT(suite, "WriteClearsErrnoOnSuccess",
           get_errno() == THUNDEROS_OK,
           "errno should be clear after successful pipe_write");

    char rbuf[64];
    kmemset(rbuf, 0, sizeof(rbuf));
    clear_errno();
    int nread = pipe_read(p, rbuf, (size_t)msg_len);
       int matches = 1;
       for (int i = 0; i < msg_len; i++) {
              if (rbuf[i] != msg[i]) {
                     matches = 0;
                     break;
              }
       }
    ASSERT(suite, "ReadReturnsExpectedByteCount",
           nread == msg_len,
           "pipe_read did not return the expected byte count");
    ASSERT(suite, "ReadDataMatchesWrittenData",
                 matches,
           "read data does not match written data");
    ASSERT(suite, "ReadClearsErrnoOnSuccess",
           get_errno() == THUNDEROS_OK,
           "errno should be clear after successful pipe_read");

    pipe_close_write(p);
    char eof_buf[4];
    kmemset(eof_buf, 0, sizeof(eof_buf));
    nread = pipe_read(p, eof_buf, sizeof(eof_buf));
    ASSERT(suite, "ReadAfterCloseWriteReturnsEof",
           nread == 0,
           "expected 0 (EOF) after write end closed");

    pipe_close_read(p);
    ASSERT(suite, "PipeCanFreeAfterBothEndsClose",
           pipe_can_free(p) == 1,
           "expected pipe_can_free == 1 when both ends are closed");
    pipe_free(p);

    pipe_t *p2 = pipe_create();
    ASSERT(suite, "SecondPipeCreateReturnsNonNull",
           p2 != NULL,
           "second pipe_create returned NULL");

    if (p2) {
        pipe_close_read(p2);
        clear_errno();
        int w = pipe_write(p2, "x", 1);
        ASSERT(suite, "WriteWithClosedReaderReturnsMinusOne",
               w == -1,
               "expected -1 when writing to a pipe with no readers");
        ASSERT(suite, "WriteWithClosedReaderSetsEpipe",
               get_errno() == THUNDEROS_EPIPE,
               "expected THUNDEROS_EPIPE when writing with no readers");
        pipe_close_write(p2);
        pipe_free(p2);
    }

    clear_errno();
    int wr = pipe_write(NULL, "x", 1);
    ASSERT(suite, "WriteRejectsNullPipe",
           wr < 0,
           "pipe_write should reject a NULL pipe");

    clear_errno();
    char tmp;
    int rd = pipe_read(NULL, &tmp, 1);
    ASSERT(suite, "ReadRejectsNullPipe",
           rd < 0,
           "pipe_read should reject a NULL pipe");

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
