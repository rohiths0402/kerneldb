#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "wal.h"
#include "table.h"

extern char current_db[64];

static uint32_t compute_checksum(const WALRecord *rec) {
    uint32_t sum = 0;
    const uint8_t *bytes = (const uint8_t *)rec;
    size_t len = sizeof(WALRecord) - sizeof(uint32_t);
    for (size_t i = 0; i < len; i++)
        sum = (sum << 1) ^ bytes[i];

    return sum;
}

static WALResult write_record(int fd, WALRecord *rec) {
    rec->checksum = compute_checksum(rec);
    ssize_t written = write(fd, rec, sizeof(WALRecord));
    if (written != (ssize_t)sizeof(WALRecord)) {
        perror("wal write_record");
        return WAL_ERROR;
    }
    return WAL_OK;
}

static WALResult read_record(int fd, WALRecord *rec) {
    ssize_t bytes = read(fd, rec, sizeof(WALRecord));
    if (bytes == 0)
        return WAL_ERROR;
    if (bytes < 0)
        { perror("wal read_record"); 
        return WAL_ERROR; }
    if (bytes != (ssize_t)sizeof(WALRecord)) 
        return WAL_CORRUPT;

    uint32_t expected = rec->checksum;
    rec->checksum = 0;
    uint32_t actual = compute_checksum(rec);
    rec->checksum = expected;

    if (actual != expected) {
        fprintf(stderr, "  [wal] checksum mismatch — record corrupt (lsn=%llu)\n", (unsigned long long)rec->lsn);
        return WAL_CORRUPT;
    }

    return WAL_OK;
}

static void redo_insert(const WALRecord *rec) {
   Table *table = table_open(rec->table);
    if (!table) {
        printf("  [wal] REDO failed — table not found: %s\n", rec->table);
        return;
    }
    char buf[WAL_MAX_DATA + 1];
    memcpy(buf, rec->data, rec->data_len);
    buf[rec->data_len] = '\0';
    table_insert_raw(table, (uint8_t *)buf, rec->data_len);
    table_flush(table);
    table_close(table);
}

WAL *wal_open(void) {
    WAL *wal = calloc(1, sizeof(WAL));
    if (!wal) return NULL;
    wal->fd = open(WAL_FILE, O_CREAT | O_RDWR, 0644);
    if (wal->fd < 0) {
        perror("wal_open");
        free(wal);
        return NULL;
    }

    wal->next_lsn = 1;
    wal->next_txn = 1;
    WALRecord rec;
    while (read_record(wal->fd, &rec) == WAL_OK) {
        if (rec.lsn >= wal->next_lsn)
            wal->next_lsn = rec.lsn + 1;
        if (rec.txn_id >= wal->next_txn)
            wal->next_txn = rec.txn_id + 1;
    }

    lseek(wal->fd, 0, SEEK_END);

    printf("  [wal] opened — next_lsn=%llu next_txn=%u\n",(unsigned long long)wal->next_lsn, wal->next_txn);

    return wal;
}

void wal_close(WAL *wal) {
    if (!wal) return;
    fsync(wal->fd);
    close(wal->fd);
    free(wal);
}

WALResult wal_begin(WAL *wal, uint32_t *txn_id) {
    if (!wal) return WAL_ERROR;

    WALRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.lsn = wal->next_lsn++;
    rec.txn_id = wal->next_txn++;
    rec.type = WAL_BEGIN;

    if (txn_id) *txn_id = rec.txn_id;

    return write_record(wal->fd, &rec);
}

WALResult wal_write(WAL *wal, uint32_t txn_id, WALRecordType type, const char *table, const uint8_t *data, uint16_t data_len) {
    if (!wal){
        return WAL_ERROR;
    }
    WALRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.lsn = wal->next_lsn++;
    rec.txn_id = txn_id;
    rec.type = (uint8_t)type;
    rec.data_len = data_len;

    strncpy(rec.table, table, WAL_TABLE_LEN - 1);

    if (data && data_len > 0) {
        uint16_t copy_len = data_len < WAL_MAX_DATA ? data_len : WAL_MAX_DATA;
        memcpy(rec.data, data, copy_len);
    }
    WALResult res = write_record(wal->fd, &rec);
    if (res != WAL_OK){
        return res;
    }
    if (fsync(wal->fd) < 0) {  
        fprintf(stderr, "  [wal] fsync failed: %s\n", strerror(errno));
        return WAL_ERROR;
    }

    return WAL_OK;
}

WALResult wal_commit(WAL *wal, uint32_t txn_id) {
    if (!wal) return WAL_ERROR;

    WALRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.lsn = wal->next_lsn++;
    rec.txn_id = txn_id;
    rec.type = WAL_COMMIT;

    WALResult result = write_record(wal->fd, &rec);
    if (result != WAL_OK) return result;
    if (fsync(wal->fd) < 0) {
        fprintf(stderr, "  [wal] fsync failed: %s\n", strerror(errno));
        return WAL_ERROR;
    }

    return WAL_OK;
}



WALResult wal_recover(WAL *wal) {
    if (!wal) return WAL_ERROR;
    printf("\n  [wal] Starting recovery...\n");
    lseek(wal->fd, 0, SEEK_SET);

    /* Read all records */
    WALRecord records[1024];
    int count = 0;
    WALRecord rec;
    while (read_record(wal->fd, &rec) == WAL_OK) {
        if (count < 1024)
            records[count++] = rec;
    } 
    if (count == 0) {
        printf("  [wal] No records found — clean start\n\n");
        lseek(wal->fd, 0, SEEK_END);
        return WAL_OK;
    }
    uint32_t committed[256];
    int commit_count = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].type == WAL_COMMIT) {
            committed[commit_count++] = records[i].txn_id;
        }
    }

    printf("  [wal] Found %d records, %d committed transactions\n", count, commit_count);
    int redone = 0;
    int skipped = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].type == WAL_BEGIN || records[i].type == WAL_COMMIT)
            continue;
        int is_committed = 0;
        for (int j = 0; j < commit_count; j++) {
            if (committed[j] == records[i].txn_id) {
                is_committed = 1;
                break;
            }
        }
        if (is_committed) {
            const char *op = records[i].type == WAL_INSERT ? "INSERT" : records[i].type == WAL_UPDATE ? "UPDATE" : "DELETE";
            printf("  [wal] REDO lsn=%-4llu txn=%-3u %-7s table=%s data=%s\n", (unsigned long long)records[i].lsn, records[i].txn_id, op, records[i].table, records[i].data);
            if(records[i].type == WAL_INSERT) {
                redo_insert(&records[i]);
            }
            redone++;
        } 
        else {
            printf("  [wal] UNDO lsn=%-4llu txn=%-3u (incomplete — skipped)\n", (unsigned long long)records[i].lsn, records[i].txn_id);
            skipped++;
        }
    }

    printf("\n  [wal] Recovery complete — %d redone, %d skipped\n\n", redone, skipped);

    /* Seek to end for appending new records */
    lseek(wal->fd, 0, SEEK_END);
    ftruncate(wal->fd, 0);
    lseek(wal->fd, 0, SEEK_SET);
    wal->next_lsn = 1;
    wal->next_txn = 1;
    printf("  [wal] Checkpoint — WAL truncated after recovery\n\n");
    return WAL_OK;
}

void wal_print(WAL *wal) {
    if (!wal) return;
    lseek(wal->fd, 0, SEEK_SET);
    static const char *TYPE_NAMES[] = { "", "BEGIN", "INSERT", "UPDATE", "DELETE", "COMMIT" };

    printf("\n  [wal] contents:\n");
    printf("  %-6s  %-6s  %-8s  %-16s  %s\n", "LSN", "TXN", "TYPE", "TABLE", "DATA");
    printf("  %-6s  %-6s  %-8s  %-16s  %s\n", "------", "------", "--------", "----------------", "----");

    WALRecord rec;
    int count = 0;
    while (read_record(wal->fd, &rec) == WAL_OK) {
        const char *type = (rec.type >= 1 && rec.type <= 5) ? TYPE_NAMES[rec.type] : "?";
        printf("  %-6llu  %-6u  %-8s  %-16s  %s\n",
               (unsigned long long)rec.lsn,
               rec.txn_id,
               type,
               rec.table,
               rec.data_len > 0 ? (char *)rec.data : "-");
        count++;
    }

    if (count == 0) printf("  (empty)\n");
    printf("\n");

    lseek(wal->fd, 0, SEEK_END);
}