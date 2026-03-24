#ifndef AIO_H
#define AIO_H

#include <stdint.h>
#include <liburing.h>


typedef void (*AIOCallback)(uint32_t page_id, int success, void *userdata);

typedef struct {
    struct io_uring ring;
    int efd;
    AIOCallback callback;
    void *userdata;
    int pending;
} AIOContext;

typedef enum {
    AIO_OK,
    AIO_ERROR,
    AIO_UNAVAILABLE
} AIOResult;

AIOResult aio_init(AIOContext *ctx, AIOCallback callback, void *userdata);
void aio_destroy(AIOContext *ctx);
AIOResult aio_write_page(AIOContext *ctx, int fd, const void *buf, uint32_t page_id);
int aio_process_completions(AIOContext *ctx);
int aio_eventfd(const AIOContext *ctx);
void aio_flush(AIOContext *ctx);

#endif