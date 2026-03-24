#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <poll.h>
#include "aio.h"
#include "../storage/page/page.h"

static inline uint64_t encode_userdata(int fd, uint32_t page_id) {
    return ((uint64_t)(uint32_t)fd << 32) | (uint64_t)page_id;
}

static inline uint32_t decode_page_id(uint64_t userdata) {
    return (uint32_t)(userdata & 0xFFFFFFFF);
}

AIOResult aio_init(AIOContext *ctx, AIOCallback callback, void *userdata) {
    if (!ctx) return AIO_ERROR;

    memset(ctx, 0, sizeof(AIOContext));
    ctx->callback = callback;
    ctx->userdata = userdata;
    ctx->pending = 0;

    int ret = io_uring_queue_init(AIO_QUEUE_DEPTH, &ctx->ring, 0);
    if (ret < 0) {
        fprintf(stderr, "  [aio] io_uring_queue_init failed: %s\n", strerror(-ret));
        return AIO_UNAVAILABLE;
    }

    /* Create eventfd - kernel will write to this when completions arrive */
    ctx->efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (ctx->efd < 0) {
        perror("  [aio] eventfd");
        io_uring_queue_exit(&ctx->ring);
        return AIO_ERROR;
    }

    ret = io_uring_register_eventfd(&ctx->ring, ctx->efd);
    if (ret < 0) {
        fprintf(stderr, "  [aio] io_uring_register_eventfd failed: %s\n", strerror(-ret));
        close(ctx->efd);
        io_uring_queue_exit(&ctx->ring);
        return AIO_ERROR;
    }

    printf("  [aio] io_uring ready — queue_depth=%d eventfd=%d\n", AIO_QUEUE_DEPTH, ctx->efd);

    return AIO_OK;
}

void aio_destroy(AIOContext *ctx) {
    if (!ctx) return;
    if (ctx->pending > 0) {
        printf("  [aio] flushing %d pending writes...\n", ctx->pending);
        aio_flush(ctx);
    }

    if (ctx->efd >= 0) close(ctx->efd);
    io_uring_queue_exit(&ctx->ring);
    ctx->efd = -1;
    ctx->pending = 0;
}

AIOResult aio_write_page(AIOContext *ctx, int fd, const void *buf, uint32_t page_id) {
    if (!ctx || fd < 0 || !buf) return AIO_ERROR;

    /* Get a submission queue entry */
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);
    if (!sqe) {
        fprintf(stderr, "  [aio] submission queue full\n");
        return AIO_ERROR;
    }
    off_t offset = (off_t)page_id * PAGE_SIZE;
    io_uring_prep_write(sqe, fd, buf, PAGE_SIZE, offset);
    io_uring_sqe_set_data64(sqe, encode_userdata(fd, page_id));

    /* Submit to kernel — non-blocking */
    int ret = io_uring_submit(&ctx->ring);
    if (ret < 0) {
        fprintf(stderr, "  [aio] io_uring_submit failed: %s\n", strerror(-ret));
        return AIO_ERROR;
    }

    ctx->pending++;
    return AIO_OK;
}

int aio_process_completions(AIOContext *ctx) {
    if (!ctx) return 0;
    uint64_t val;
    if (read(ctx->efd, &val, sizeof(val)) < 0 && errno != EAGAIN) {
        perror("  [aio] eventfd read");
    }
    struct io_uring_cqe *cqe;
    int processed = 0;

    /* Drain all completions */
    while (io_uring_peek_cqe(&ctx->ring, &cqe) == 0) {
        uint32_t page_id = decode_page_id(io_uring_cqe_get_data64(cqe));
        int success = (cqe->res == PAGE_SIZE) ? 1 : 0;
        if (!success) {
            fprintf(stderr, "  [aio] write failed for page %u: %s\n", page_id, strerror(-cqe->res));
        }

        /* Notify caller */
        if (ctx->callback)
            ctx->callback(page_id, success, ctx->userdata);

        io_uring_cqe_seen(&ctx->ring, cqe);
        ctx->pending--;
        processed++;
    }

    return processed;
}

int aio_eventfd(const AIOContext *ctx) {
    if (!ctx) return -1;
    return ctx->efd;
}

void aio_flush(AIOContext *ctx) {
    if (!ctx) return;

    while (ctx->pending > 0) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ctx->ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "  [aio] flush wait failed: %s\n", strerror(-ret));
            break;
        }

        uint32_t page_id = decode_page_id(io_uring_cqe_get_data64(cqe));
        int success = (cqe->res == PAGE_SIZE) ? 1 : 0;

        if (ctx->callback)
            ctx->callback(page_id, success, ctx->userdata);

        io_uring_cqe_seen(&ctx->ring, cqe);
        ctx->pending--;
    }
}