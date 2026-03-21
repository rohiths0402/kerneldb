#ifndef BUFFER_H
#define BUFFER_H
#include <stdint.h>
#include "../page/page.h"
#define BUFFER_POOL_SIZE  7
#define INVALID_PAGE_ID   UINT32_MAX

typedef struct {
    Page    *page;
    uint32_t page_id;
    int      dirty;
    uint64_t lru_rank;
} BufferFrame;

typedef struct {
    BufferFrame frames[BUFFER_POOL_SIZE];
    uint64_t    clock;
    int         table_fd;
} BufferPool;

typedef enum {
    BUF_OK,
    BUF_ERROR,
    BUF_NOT_FOUND
} BufResult;

void buf_init(BufferPool *pool, int table_fd);

void buf_destroy(BufferPool *pool);

BufferFrame *buf_pin(BufferPool *pool, uint32_t page_id);

void buf_mark_dirty(BufferFrame *frame);

BufResult buf_flush_all(BufferPool *pool);

BufferFrame *buf_new_page(BufferPool *pool, uint32_t page_id);

void buf_print(const BufferPool *pool);

#endif