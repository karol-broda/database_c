#ifndef DATABASE_H
#define DATABASE_H

#include "buffer.h"
#include "wal.h"
#include "catalog.h"

typedef struct {
    BufferPool* pool;
    Pager* pager;
    Wal* wal;
    uint32_t root_page_id;
    uint32_t current_tx_id;
    int locked;
    Catalog* catalog;
} Database;


typedef struct {
    int num_rows;
    char*** rows;
} Result;

Database* db_open(const char* filename);
void db_close(Database* db);
Result* db_execute(Database* db, const char* query);

typedef struct {
    char** headers;
    char*** rows;
    int num_columns;
    int num_rows;
    char* title;
} TablePrinter;

TablePrinter* table_printer_create(const char* title, int num_columns);
void table_printer_add_header(TablePrinter* printer, int col, const char* header);
void table_printer_add_row(TablePrinter* printer, char** row_data);
void table_printer_print(TablePrinter* printer);
void table_printer_free(TablePrinter* printer);

void db_help(void);
void db_list_databases(const char* filename);
void db_list_tables(Database* db);
void db_describe_table(Database* db, const char* table_name);
void db_describe_all(Database* db);

#endif // DATABASE_H
