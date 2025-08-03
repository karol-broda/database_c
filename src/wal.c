#include "wal.h"
#include "btree.h"
#include "buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Wal* wal_init(const char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        return NULL;
    }

    Wal* wal = (Wal*)malloc(sizeof(Wal));
    if (wal == NULL) {
        close(fd);
        return NULL;
    }

    wal->fd = fd;
    wal->lsn = 0;

    return wal;
}

void wal_close(Wal* wal) {
    if (wal != NULL) {
        close(wal->fd);
        free(wal);
    }
}

void wal_log_insert(Wal* wal, uint32_t tx_id, int key, const char* value) {
    uint16_t value_len = strlen(value) + 1;
    LogRecordHeader header = {wal->lsn++, (uint32_t)LOG_RECORD_TYPE_INSERT, tx_id, value_len};
    write(wal->fd, &header, sizeof(LogRecordHeader));
    write(wal->fd, &key, sizeof(int));
    write(wal->fd, value, value_len);
}

void wal_log_delete(Wal* wal, uint32_t tx_id, int key) {
    LogRecordHeader header = {wal->lsn++, (uint32_t)LOG_RECORD_TYPE_DELETE, tx_id, 0};
    write(wal->fd, &header, sizeof(LogRecordHeader));
    write(wal->fd, &key, sizeof(int));
}

void wal_log_update(Wal* wal, uint32_t tx_id, int key, const char* value) {
    uint16_t value_len = strlen(value) + 1;
    LogRecordHeader header = {wal->lsn++, (uint32_t)LOG_RECORD_TYPE_UPDATE, tx_id, value_len};
    write(wal->fd, &header, sizeof(LogRecordHeader));
    write(wal->fd, &key, sizeof(int));
    write(wal->fd, value, value_len);
}

void wal_log_commit(Wal* wal, uint32_t tx_id) {
    LogRecordHeader header = {wal->lsn++, (uint32_t)LOG_RECORD_TYPE_COMMIT, tx_id, 0};
    write(wal->fd, &header, sizeof(LogRecordHeader));
}

void wal_log_begin(Wal* wal, uint32_t tx_id) {
    LogRecordHeader header = {wal->lsn++, LOG_RECORD_TYPE_BEGIN, tx_id, 0};
    write(wal->fd, &header, sizeof(LogRecordHeader));
}

uint32_t* wal_get_committed_tx_ids(Wal* wal, int* count) {
    lseek(wal->fd, 0, SEEK_SET);
    uint32_t* committed_tx_ids = NULL;
    *count = 0;

    LogRecordHeader header;
    while (read(wal->fd, &header, sizeof(LogRecordHeader)) > 0) {
        
        if ((LogRecordType)header.type == LOG_RECORD_TYPE_COMMIT) {
            (*count)++;
            committed_tx_ids = (uint32_t*)realloc(committed_tx_ids, sizeof(uint32_t) * (*count));
            committed_tx_ids[*count - 1] = header.tx_id;
        } else if ((LogRecordType)header.type == LOG_RECORD_TYPE_INSERT) {
            int key;
            char value[256];
            read(wal->fd, &key, sizeof(int));
            if (header.value_len > 0) {
                if (header.value_len > 255) {
                    // skip oversized value
                    lseek(wal->fd, header.value_len, SEEK_CUR);
                } else {
                    read(wal->fd, value, header.value_len);
                    value[header.value_len] = '\0'; 
                }
            }
        } else if ((LogRecordType)header.type == LOG_RECORD_TYPE_DELETE) {
            int key;
            read(wal->fd, &key, sizeof(int));
        }
    }
    return committed_tx_ids;
}

void wal_recover(BufferPool* pool, Pager* pager, Wal* wal, uint32_t root_page_id) {
    int num_committed_tx = 0;
    uint32_t* committed_tx_ids = wal_get_committed_tx_ids(wal, &num_committed_tx);

    

    lseek(wal->fd, 0, SEEK_SET);

    LogRecordHeader header;
    while (read(wal->fd, &header, sizeof(LogRecordHeader)) > 0) {
        
        int is_committed = 0;
        for (int i = 0; i < num_committed_tx; i++) {
            if (header.tx_id == committed_tx_ids[i]) {
                is_committed = 1;
                break;
            }
        }

        if (is_committed) {
            
            if ((LogRecordType)header.type == LOG_RECORD_TYPE_INSERT) {
                int key;
                char value[256];
                read(wal->fd, &key, sizeof(int));
                if (header.value_len > 0) {
                    if (header.value_len > 255) {
                        // skip oversized value
                        lseek(wal->fd, header.value_len, SEEK_CUR);
                        value[0] = '\0'; 
                    } else {
                        read(wal->fd, value, header.value_len);
                        value[header.value_len] = '\0'; 
                    }
                }
                
                btree_insert(pool, pager, root_page_id, key, value, 0);
            } else if ((LogRecordType)header.type == LOG_RECORD_TYPE_DELETE) {
                int key;
                read(wal->fd, &key, sizeof(int));
                btree_delete(pool, pager, root_page_id, key, 0);
            } else if ((LogRecordType)header.type == LOG_RECORD_TYPE_UPDATE) {
                int key;
                char value[256];
                read(wal->fd, &key, sizeof(int));
                if (header.value_len > 0) {
                    if (header.value_len > 255) {
                        // skip oversized value
                        lseek(wal->fd, header.value_len, SEEK_CUR);
                        value[0] = '\0'; 
                    } else {
                        read(wal->fd, value, header.value_len);
                        value[header.value_len] = '\0'; 
                    }
                }
                btree_delete(pool, pager, root_page_id, key, 0);
                btree_insert(pool, pager, root_page_id, key, value, 0);
            }
        } else { // skip uncommitted data
            
            if ((LogRecordType)header.type == LOG_RECORD_TYPE_INSERT) {
                int key;
                char value[256];
                read(wal->fd, &key, sizeof(int));
                if (header.value_len > 0) {
                    read(wal->fd, value, header.value_len);
                }
            } else if ((LogRecordType)header.type == LOG_RECORD_TYPE_DELETE) {
                int key;
                read(wal->fd, &key, sizeof(int));
            }
        }
    }
    if (committed_tx_ids != NULL) {
        free(committed_tx_ids);
    }
}

void wal_apply_committed_transactions(BufferPool* pool, Pager* pager, uint32_t root_page_id, uint32_t tx_id) {
    lseek(pager->fd, 0, SEEK_SET);

    LogRecordHeader header;
    while (read(pager->fd, &header, sizeof(LogRecordHeader)) > 0) {
        
        if (header.tx_id == tx_id && (LogRecordType)header.type == LOG_RECORD_TYPE_INSERT) {
            int key;
            char value[256];
            read(pager->fd, &key, sizeof(int));
            if (header.value_len > 0) {
                if (header.value_len > 255) {
                    // skip oversized value
                    lseek(pager->fd, header.value_len, SEEK_CUR);
                    value[0] = '\0'; 
                } else {
                    read(pager->fd, value, header.value_len);
                    value[header.value_len] = '\0'; 
                }
            }
            
            btree_insert(pool, pager, root_page_id, key, value, 0);
        } else if (header.tx_id == tx_id && (LogRecordType)header.type == LOG_RECORD_TYPE_DELETE) {
            int key;
            read(pager->fd, &key, sizeof(int));
            
            btree_delete(pool, pager, root_page_id, key, 0);
        } else if (header.tx_id == tx_id && (LogRecordType)header.type == LOG_RECORD_TYPE_UPDATE) {
            int key;
            char value[256];
            read(pager->fd, &key, sizeof(int));
            if (header.value_len > 0) {
                if (header.value_len > 255) {
                    // skip oversized value
                    lseek(pager->fd, header.value_len, SEEK_CUR);
                    value[0] = '\0'; 
                } else {
                    read(pager->fd, value, header.value_len);
                    value[header.value_len] = '\0'; 
                }
            }
            btree_delete(pool, pager, root_page_id, key, 0);
            btree_insert(pool, pager, root_page_id, key, value, 0);
        }
    }
}