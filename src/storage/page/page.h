#ifndef PAGE_H
#define PAGE_H
#include <stdint.h>
#include <stddef.h>
#define PAGE_SIZE        4096
#define MAX_SLOTS        128
#define PAGE_HEADER_SIZE sizeof(PageHeader)
#define SLOT_ALIVE  0x01
#define SLOT_DEAD   0x02

typedef struct {
    uint16_t offset;
    uint16_t length;
    uint8_t  flags;
} Slot;

typedef struct {
    uint32_t page_id;
    uint64_t page_lsn;
    uint16_t slot_count;
    uint16_t free_start;
    uint16_t free_end;
    uint32_t checksum;
    uint8_t  _pad[10];
} PageHeader;

typedef struct {
    uint8_t data[PAGE_SIZE];
} Page;

typedef enum {
    PAGE_OK,
    PAGE_FULL,
    PAGE_ERROR,
    PAGE_NOT_FOUND
} PageResult;

Page *page_alloc(uint32_t page_id);

void page_free(Page *page);

PageResult page_insert(Page *page, const uint8_t *data, uint16_t length, uint16_t *slot_out);

PageResult page_read(const Page *page, uint16_t slot_idx, uint8_t *buf, uint16_t buf_size, uint16_t *len_out);

PageResult page_delete(Page *page, uint16_t slot_idx);

void page_compact(Page *page);

PageResult page_write_disk(const Page *page, int fd);

PageResult page_read_disk(Page *page, int fd, uint32_t page_id);

void page_print(const Page *page);

#endif