#ifndef WAL_H
#define WAL_H

#include <stdint.h>
#include "buffer.h"


typedef enum {
    LOG_RECORD_TYPE_INSERT,
    LOG_RECORD_TYPE_DELETE,
    LOG_RECORD_TYPE_UPDATE,
    LOG_RECORD_TYPE_COMMIT,
    LOG_RECORD_TYPE_BEGIN,
    LOG_RECORD_TYPE_CHECKPOINT
} LogRecordType;


typedef struct {
    uint64_t lsn;
    uint32_t type;
    uint32_t tx_id;
    uint16_t value_len;
} LogRecordHeader; __attribute__((packed)); __attribute__((packed));


typedef struct {
    int fd;
    uint64_t lsn;
} Wal;

Wal* wal_init(const char* filename);
void wal_close(Wal* wal);
void wal_log_insert(Wal* wal, uint32_t tx_id, int key, const char* value);
void wal_log_delete(Wal* wal, uint32_t tx_id, int key);
void wal_log_update(Wal* wal, uint32_t tx_id, int key, const char* value);
void wal_log_commit(Wal* wal, uint32_t tx_id);
void wal_log_begin(Wal* wal, uint32_t tx_id);
void wal_recover(BufferPool* pool, Pager* pager, Wal* wal, uint32_t root_page_id);
void wal_apply_committed_transactions(BufferPool* pool, Pager* pager, uint32_t root_page_id, uint32_t tx_id);
uint32_t* wal_get_committed_tx_ids(Wal* wal, int* count);


#endif // WAL_H
