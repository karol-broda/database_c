#ifndef CATALOG_H
#define CATALOG_H

#include <stdint.h>

#define MAX_TABLES 16
#define MAX_COLUMNS_PER_TABLE 16
#define MAX_NAME_LEN 64

typedef enum {
    COLUMN_TYPE_INT,
    COLUMN_TYPE_VARCHAR
} ColumnType;

typedef struct {
    char name[MAX_NAME_LEN];
    ColumnType type;
    uint16_t length; // for VARCHAR
    int is_primary_key;
} ColumnSchema;

typedef struct {
    char table_name[MAX_NAME_LEN];
    uint32_t root_page_id;
    uint16_t num_columns;
    ColumnSchema columns[MAX_COLUMNS_PER_TABLE];
} TableSchema;

typedef struct {
    uint16_t num_tables;
    TableSchema tables[MAX_TABLES];
} Catalog;

#endif // CATALOG_H
