#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "page.h"

static inline PageHeader *get_header(Page *page) {
    return (PageHeader *)page->data;
}

static inline const PageHeader *get_header_c(const Page *page) {
    return (const PageHeader *)page->data;
}

static inline Slot *get_slots(Page *page) {
    return (Slot *)(page->data + PAGE_HEADER_SIZE);
}

static inline const Slot *get_slots_c(const Page *page) {
    return (const Slot *)(page->data + PAGE_HEADER_SIZE);
}

static uint32_t compute_checksum(const Page *page) {
    uint32_t sum = 0;
    for (size_t i = PAGE_HEADER_SIZE; i < PAGE_SIZE; i++)
        sum = (sum << 1) ^ page->data[i];
    return sum;
}

static uint16_t free_space(const Page *page) {
    const PageHeader *hdr = get_header_c(page);
    if (hdr->free_end <= hdr->free_start)
        return 0;
    return (uint16_t)(hdr->free_end - hdr->free_start);
}

Page *page_alloc(uint32_t page_id) {
    void *mem = NULL;
    if (posix_memalign(&mem, PAGE_SIZE, PAGE_SIZE) != 0) {
        perror("posix_memalign");
        return NULL;
    }

    Page *page = (Page *)mem;
    memset(page->data, 0, PAGE_SIZE);
    PageHeader *hdr   = get_header(page);
    hdr->page_id      = page_id;
    hdr->slot_count   = 0;
    hdr->free_start   = (uint16_t)(PAGE_HEADER_SIZE);
    hdr->free_end     = (uint16_t)(PAGE_SIZE);
    hdr->checksum     = 0;

    return page;
}

void page_free(Page *page) {
    if (page) free(page);
}

PageResult page_insert(Page *page, const uint8_t *data, uint16_t length,
                       uint16_t *slot_out) {
    PageHeader *hdr = get_header(page);
    uint16_t needed = (uint16_t)(sizeof(Slot) + length);
    if (free_space(page) < needed)
        return PAGE_FULL;

    if (hdr->slot_count >= MAX_SLOTS)
        return PAGE_FULL;
    uint16_t row_offset = (uint16_t)(hdr->free_end - length);
    memcpy(page->data + row_offset, data, length);
    hdr->free_end = row_offset;
    Slot *slots = get_slots(page);
    uint16_t slot_idx = hdr->slot_count;
    slots[slot_idx].offset = row_offset;
    slots[slot_idx].length = length;
    slots[slot_idx].flags  = SLOT_ALIVE;

    hdr->slot_count++;
    hdr->free_start = (uint16_t)(PAGE_HEADER_SIZE + hdr->slot_count * sizeof(Slot));

    if (slot_out) *slot_out = slot_idx;
    return PAGE_OK;
}

PageResult page_read(const Page *page, uint16_t slot_idx, uint8_t *buf, uint16_t buf_size, uint16_t *len_out) {
    const PageHeader *hdr   = get_header_c(page);
    const Slot       *slots = get_slots_c(page);

    if (slot_idx >= hdr->slot_count)
        return PAGE_NOT_FOUND;

    const Slot *slot = &slots[slot_idx];

    if (!(slot->flags & SLOT_ALIVE))
        return PAGE_NOT_FOUND;

    if (slot->length > buf_size)
        return PAGE_ERROR;

    memcpy(buf, page->data + slot->offset, slot->length);
    if (len_out) *len_out = slot->length;

    return PAGE_OK;
}

PageResult page_delete(Page *page, uint16_t slot_idx) {
    PageHeader *hdr   = get_header(page);
    Slot       *slots = get_slots(page);

    if (slot_idx >= hdr->slot_count)
        return PAGE_NOT_FOUND;

    if (!(slots[slot_idx].flags & SLOT_ALIVE))
        return PAGE_NOT_FOUND;

    slots[slot_idx].flags = SLOT_DEAD;
    return PAGE_OK;
}

void page_compact(Page *page) {
    PageHeader *hdr   = get_header(page);
    Slot       *slots = get_slots(page);
    uint8_t tmp[PAGE_SIZE];
    memset(tmp, 0, PAGE_SIZE);
    PageHeader *tmp_hdr = (PageHeader *)tmp;
    *tmp_hdr = *hdr;
    tmp_hdr->slot_count = 0;
    tmp_hdr->free_start = (uint16_t)PAGE_HEADER_SIZE;
    tmp_hdr->free_end   = (uint16_t)PAGE_SIZE;

    Slot *tmp_slots = (Slot *)(tmp + PAGE_HEADER_SIZE);
    for (uint16_t i = 0; i < hdr->slot_count; i++) {
        if (!(slots[i].flags & SLOT_ALIVE)) continue;
        uint16_t len    = slots[i].length;
        uint16_t newoff = (uint16_t)(tmp_hdr->free_end - len);
        memcpy(tmp + newoff, page->data + slots[i].offset, len);
        tmp_hdr->free_end = newoff;
        tmp_slots[tmp_hdr->slot_count].offset = newoff;
        tmp_slots[tmp_hdr->slot_count].length = len;
        tmp_slots[tmp_hdr->slot_count].flags  = SLOT_ALIVE;
        tmp_hdr->slot_count++;
    }

    tmp_hdr->free_start = (uint16_t)(PAGE_HEADER_SIZE + tmp_hdr->slot_count * sizeof(Slot));
    memcpy(page->data, tmp, PAGE_SIZE);
}

PageResult page_write_disk(const Page *page, int fd) {
    const PageHeader *hdr = get_header_c(page);
    ((Page *)page)->data[0] = page->data[0];
    uint32_t cs = compute_checksum(page);
    ((PageHeader *)hdr)->checksum = cs;
    off_t offset = (off_t)hdr->page_id * PAGE_SIZE;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        perror("lseek");
        return PAGE_ERROR;
    }
    ssize_t written = write(fd, page->data, PAGE_SIZE);
    if (written != PAGE_SIZE) {
        perror("write");
        return PAGE_ERROR;
    }
    return PAGE_OK;
}

PageResult page_read_disk(Page *page, int fd, uint32_t page_id) {
    off_t offset = (off_t)page_id * PAGE_SIZE;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        perror("lseek");
        return PAGE_ERROR;
    }

    ssize_t bytes = read(fd, page->data, PAGE_SIZE);
    if (bytes != PAGE_SIZE) {
        perror("read");
        return PAGE_ERROR;
    }
    PageHeader *hdr  = get_header(page);
    uint32_t expected = hdr->checksum;
    hdr->checksum     = 0;
    uint32_t actual   = compute_checksum(page);
    hdr->checksum     = expected;

    if (actual != expected) {
        fprintf(stderr, "  [page] checksum mismatch on page %u\n", page_id);
        return PAGE_ERROR;
    }
    return PAGE_OK;
}

void page_print(const Page *page) {
    const PageHeader *hdr   = get_header_c(page);
    const Slot       *slots = get_slots_c(page);

    printf("\n  [page] id=%u  slots=%u  free=%u bytes\n", hdr->page_id, hdr->slot_count,(unsigned)(hdr->free_end - hdr->free_start));

    for (uint16_t i = 0; i < hdr->slot_count; i++) {
        const char *status = (slots[i].flags & SLOT_ALIVE) ? "alive" : "dead";
        printf("    slot[%2u]  offset=%-5u  len=%-4u  %s\n",i, slots[i].offset, slots[i].length, status);

        if (slots[i].flags & SLOT_ALIVE) {
            /* Print row as string (assumes null-terminated or printable) */
            printf("             data: \"%.*s\"\n",slots[i].length, page->data + slots[i].offset);
        }
    }
    printf("\n");
}