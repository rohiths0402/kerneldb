#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "buffer.h"
#include "../page/page.h"

static int find_frame(const BufferPool *pool, uint32_t page_id) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id &&
            pool->frames[i].page   != NULL)
            return i;
    }
    return -1;
}

static int find_empty(const BufferPool *pool) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page == NULL)
            return i;
    }
    return -1;
}

static int find_lru(const BufferPool *pool) {
    int      lru_idx  = 0;
    uint64_t lru_rank = pool->frames[0].lru_rank;

    for (int i = 1; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].lru_rank < lru_rank) {
            lru_rank = pool->frames[i].lru_rank;
            lru_idx  = i;
        }
    }
    return lru_idx;
}

static BufResult evict_frame(BufferPool *pool, int idx) {
    BufferFrame *frame = &pool->frames[idx];

    if (frame->page == NULL) return BUF_OK;

    /* Flush if dirty */
    if (frame->dirty) {
        PageResult pr = page_write_disk(frame->page, pool->table_fd);
        if (pr != PAGE_OK) {
            /* WAL is the safety net — log and continue */
            fprintf(stderr,"  [buffer] WARNING: flush failed for page %u — WAL will recover\n",frame->page_id);
            return BUF_ERROR;
        }
        if (fsync(pool->table_fd) < 0) {
            fprintf(stderr,"  [buffer] WARNING: fsync failed — WAL will recover\n");
            
        }
    }

    page_free(frame->page);
    frame->page    = NULL;
    frame->page_id = INVALID_PAGE_ID;
    frame->dirty   = 0;
    frame->lru_rank = 0;

    return BUF_OK;
}

void buf_init(BufferPool *pool, int table_fd) {
    memset(pool, 0, sizeof(BufferPool));
    pool->table_fd = table_fd;
    pool->clock    = 1;

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        pool->frames[i].page    = NULL;
        pool->frames[i].page_id = INVALID_PAGE_ID;
        pool->frames[i].dirty   = 0;
        pool->frames[i].lru_rank = 0;
    }
}

void buf_destroy(BufferPool *pool) {
    buf_flush_all(pool);
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page) {
            page_free(pool->frames[i].page);
            pool->frames[i].page = NULL;
        }
    }
}

BufferFrame *buf_pin(BufferPool *pool, uint32_t page_id) {
    int idx = find_frame(pool, page_id);
    if (idx >= 0) {
        pool->frames[idx].lru_rank = pool->clock++;
        return &pool->frames[idx];
    }
    int slot = find_empty(pool);

    if (slot < 0) {
        slot = find_lru(pool);
        if (evict_frame(pool, slot) != BUF_OK)
            return NULL;
    }
    Page *page = page_alloc(page_id);
    if (!page) return NULL;

    if (page_read_disk(page, pool->table_fd, page_id) != PAGE_OK) {
        page_free(page);
        return NULL;
    }

    pool->frames[slot].page     = page;
    pool->frames[slot].page_id  = page_id;
    pool->frames[slot].dirty    = 0;
    pool->frames[slot].lru_rank = pool->clock++;

    return &pool->frames[slot];
}

void buf_mark_dirty(BufferFrame *frame) {
    if (frame) frame->dirty = 1;
}

BufResult buf_flush_all(BufferPool *pool) {
    BufResult result = BUF_OK;

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page && pool->frames[i].dirty) {
            if (page_write_disk(pool->frames[i].page, pool->table_fd) != PAGE_OK) {
                fprintf(stderr,"  [buffer] flush_all: failed on page %u\n",pool->frames[i].page_id);
                result = BUF_ERROR;
                continue;
            }
            pool->frames[i].dirty = 0;
        }
    }

    if (result == BUF_OK)
        fsync(pool->table_fd);

    return result;
}
BufferFrame *buf_new_page(BufferPool *pool, uint32_t page_id) {
    int slot = find_empty(pool);
    if (slot < 0) {
        slot = find_lru(pool);
        if (evict_frame(pool, slot) != BUF_OK)
            return NULL;
    }

    Page *page = page_alloc(page_id);
    if (!page) return NULL;

    pool->frames[slot].page     = page;
    pool->frames[slot].page_id  = page_id;
    pool->frames[slot].dirty    = 1;
    pool->frames[slot].lru_rank = pool->clock++;

    return &pool->frames[slot];
}

void buf_print(const BufferPool *pool) {
    printf("\n  [buffer pool]  frames=%d  clock=%llu\n",BUFFER_POOL_SIZE, (unsigned long long)pool->clock);
    printf("  %-6s  %-10s  %-6s  %-10s\n","Frame", "PageID", "Dirty", "LRU Rank");
    printf("  %-6s  %-10s  %-6s  %-10s\n","-----", "------", "-----", "--------");

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        const BufferFrame *f = &pool->frames[i];
        if (f->page == NULL) {
            printf("  %-6d  %-10s  %-6s  %-10s\n", i, "(empty)", "-", "-");
        } else {
            printf("  %-6d  %-10u  %-6s  %-10llu\n",i, f->page_id,f->dirty ? "YES" : "no", (unsigned long long)f->lru_rank);
        }
    }
    printf("\n");
}