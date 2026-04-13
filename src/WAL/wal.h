#ifndef WAL_H
#define WAL_H

#include <stdint.h>
#define WAL_FILE "data/wal.log"
#define WAL_MAX_DATA 256
#define WAL_TABLE_LEN 64

typedef enum {
    WAL_BEGIN = 1,
    WAL_INSERT = 2,
    WAL_UPDATE = 3,
    WAL_DELETE = 4,
    WAL_COMMIT = 5
} WALRecordType;

// typedef struct {
//     uint64_t lsn;
//     uint32_t txn_id;    
//     uint32_t page_id;    
//     uint8_t  type;
//     uint16_t before_len;
//     uint16_t after_len;
//     uint8_t  before[WAL_MAX_DATA];
//     uint8_t  after[WAL_MAX_DATA];
//     uint32_t checksum;
// } WALRecord;

typedef struct {
    uint64_t lsn;
    uint32_t txn_id;

    uint32_t page_id;   // NEW (future use)

    uint8_t  type;

    char table[WAL_TABLE_LEN];   // ✅ KEEP (for now)

    uint16_t data_len;

    uint8_t  data[WAL_MAX_DATA]; // ✅ KEEP (for now)

    uint32_t checksum;
} WALRecord;

typedef struct {
    int fd; 
    uint64_t next_lsn;
    uint32_t next_txn;
} WAL;

typedef enum {
    WAL_OK,
    WAL_ERROR,
    WAL_CORRUPT
} WALResult;

WAL *wal_open(void);
void wal_close(WAL *wal);
WALResult wal_begin(WAL *wal, uint32_t *txn_id);
WALResult wal_write(WAL *wal, uint32_t txn_id, WALRecordType type, const char *table, const uint8_t *data, uint16_t data_len);
WALResult wal_commit(WAL *wal, uint32_t txn_id);
WALResult wal_recover(WAL *wal);
void wal_print(WAL *wal);

#endif