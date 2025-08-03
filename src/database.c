#include "database.h"
#include "btree.h"
#include "wal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Database* db_open(const char* filename) {
    Database* db = (Database*)malloc(sizeof(Database));
    if (db == NULL) {
        return NULL;
    }

    db->pager = pager_open(filename);
    if (db->pager == NULL) {
        free(db);
        return NULL;
    }

    db->pool = buffer_pool_init();
    if (db->pool == NULL) {
        pager_close(db->pager);
        free(db);
        return NULL;
    }

    db->wal = wal_init("wal.log");
    if (db->wal == NULL) {
        pager_close(db->pager);
        free(db->pool);
        free(db);
        return NULL;
    }

    db->root_page_id = 0;

    if (db->pager->next_page_id == 0) {
        // new database, create root page
        Page* root_page = buffer_pool_get_page(db->pool, db->pager, 0);
        BTreeLeafNode* root_node = (BTreeLeafNode*)root_page;
        root_node->header.is_leaf = 1;
        root_node->header.num_keys = 0;
        root_node->next_leaf = 0;
        db->pager->next_page_id++;
        buffer_pool_unpin_page(db->pool, db->pager, 0, 1);

        // initialize catalog on page 1
        Page* catalog_page = buffer_pool_get_page(db->pool, db->pager, 1);
        db->catalog = (Catalog*)malloc(sizeof(Catalog));
        memcpy(db->catalog, catalog_page->data, sizeof(Catalog));
        db->catalog->num_tables = 0;
        db->pager->next_page_id++;
        buffer_pool_unpin_page(db->pool, db->pager, 1, 1);
    } else {
        // load catalog from page 1
        Page* catalog_page = buffer_pool_get_page(db->pool, db->pager, 1);
        db->catalog = (Catalog*)malloc(sizeof(Catalog));
        memcpy(db->catalog, catalog_page->data, sizeof(Catalog));
        buffer_pool_unpin_page(db->pool, db->pager, 1, 0);
    }

    // recover all tables, not just the root page
    for (int i = 0; i < db->catalog->num_tables; i++) {
        wal_recover(db->pool, db->pager, db->wal, db->catalog->tables[i].root_page_id);
    }

    db->current_tx_id = 0;
    db->locked = 0;

    return db;
}

void db_close(Database* db) {
    if (db != NULL) {
        // flush catalog to disk
        Page* catalog_page = buffer_pool_get_page(db->pool, db->pager, 1);
        memcpy(catalog_page->data, db->catalog, sizeof(Catalog));
        buffer_pool_unpin_page(db->pool, db->pager, 1, 1);

        buffer_pool_flush_all(db->pool, db->pager);
        wal_close(db->wal);
        pager_close(db->pager);
        free(db->pool);
        free(db->catalog);
        free(db);
    }
}

Result* db_execute(Database* db, const char* query) {
    if (strncmp(query, "BEGIN", 5) == 0) {
        if (db->locked) {
            fprintf(stderr, "Error: Another transaction is already in progress.\n");
            return NULL;
        }
        db->locked = 1;
        db->current_tx_id++;
        wal_log_begin(db->wal, db->current_tx_id);
        return NULL;
    } else if (strncmp(query, "COMMIT", 6) == 0) {
        if (!db->locked) {
            fprintf(stderr, "Error: No active transaction to commit.\n");
            return NULL;
        }
        wal_log_commit(db->wal, db->current_tx_id);
        buffer_pool_flush_all(db->pool, db->pager);
        db->locked = 0;
        return NULL;
    } else if (strncmp(query, "ROLLBACK", 8) == 0) {
        if (!db->locked) {
            fprintf(stderr, "Error: No active transaction to rollback.\n");
            return NULL;
        }
        // rollback by reinitializing buffer pool
        char filename_copy[256];
        strcpy(filename_copy, db->pager->filename);

        pager_close(db->pager);
        free(db->pool);

        db->pager = pager_open(filename_copy);
        db->pool = buffer_pool_init();
        
        // reload catalog from disk after reopening pager
        Page* catalog_page = buffer_pool_get_page(db->pool, db->pager, 1);
        memcpy(db->catalog, catalog_page->data, sizeof(Catalog));
        buffer_pool_unpin_page(db->pool, db->pager, 1, 0);
        
        // recover all tables, not just the root page
        for (int i = 0; i < db->catalog->num_tables; i++) {
            wal_recover(db->pool, db->pager, db->wal, db->catalog->tables[i].root_page_id);
        }

        if (db->pager == NULL || db->pool == NULL) {
            fprintf(stderr, "Error during rollback: Failed to re-initialize database components.\n");
            exit(EXIT_FAILURE);
        }
        db->locked = 0;
        return NULL;
    } else if (strncmp(query, "CREATE TABLE", 12) == 0) {
        if (db->locked) {
            fprintf(stderr, "Error: CREATE TABLE statements must not be within a transaction.\n");
            return NULL;
        }
        char table_name[MAX_NAME_LEN];
        char column_defs[256];
        
        // find the opening parenthesis
        char* start = strchr(query, '(');
        if (start == NULL) {
            fprintf(stderr, "Error: Invalid CREATE TABLE syntax.\n");
            return NULL;
        }
        start++; // skip the opening parenthesis
        
        // find the matching closing parenthesis
        char* end = strrchr(query, ')');
        if (end == NULL || end <= start) {
            fprintf(stderr, "Error: Invalid CREATE TABLE syntax.\n");
            return NULL;
        }
        
        // extract table name
        sscanf(query, "CREATE TABLE %s", table_name);
        
        // extract column definitions
        int len = end - start;
        if (len >= 256) {
            fprintf(stderr, "Error: Column definitions too long.\n");
            return NULL;
        }
        strncpy(column_defs, start, len);
        column_defs[len] = '\0';

        TableSchema new_table;
        strcpy(new_table.table_name, table_name);
        new_table.root_page_id = db->pager->next_page_id; // assign a new root page id
        new_table.num_columns = 0;

        // parse column definitions
        char* token = strtok(column_defs, ",");
        while (token != NULL && new_table.num_columns < MAX_COLUMNS_PER_TABLE) {
            // trim leading whitespace
            while (*token == ' ' || *token == '\t') {
                token++;
            }
            
            char col_name[MAX_NAME_LEN];
            char col_type[MAX_NAME_LEN];
            sscanf(token, "%s %s", col_name, col_type);

            ColumnSchema new_column;
            strcpy(new_column.name, col_name);
            if (strcmp(col_type, "INT") == 0) {
                new_column.type = COLUMN_TYPE_INT;
                new_column.length = 0;
            } else if (strncmp(col_type, "VARCHAR", 7) == 0) {
                sscanf(col_type, "VARCHAR(%hu)", &new_column.length);
                new_column.type = COLUMN_TYPE_VARCHAR;
            }
            new_column.is_primary_key = (strstr(token, "PRIMARY KEY") != NULL);

            new_table.columns[new_table.num_columns++] = new_column;
            token = strtok(NULL, ",");
        }

        // add table to catalog
        if (db->catalog->num_tables < MAX_TABLES) {
            db->catalog->tables[db->catalog->num_tables++] = new_table;

            // create root page for new table
            Page* root_page = buffer_pool_get_page(db->pool, db->pager, new_table.root_page_id);
            BTreeLeafNode* root_node = (BTreeLeafNode*)root_page;
            root_node->header.is_leaf = 1;
            root_node->header.num_keys = 0;
            root_node->next_leaf = 0;
            db->pager->next_page_id++;
            buffer_pool_unpin_page(db->pool, db->pager, new_table.root_page_id, 1);
        } else {
            fprintf(stderr, "Error: Maximum number of tables reached.\n");
        }
        return NULL;
    } else if (strncmp(query, "INSERT INTO", 11) == 0) {
        if (!db->locked) {
            fprintf(stderr, "Error: INSERT statements must be within a transaction.\n");
            return NULL;
        }
        char table_name[MAX_NAME_LEN];
        int id;
        char name[256];
        sscanf(query, "INSERT INTO %s VALUES (%d, '%[^']')", table_name, &id, name);

        uint32_t table_root_page_id = 0;
        for (int i = 0; i < db->catalog->num_tables; i++) {
            if (strcmp(db->catalog->tables[i].table_name, table_name) == 0) {
                table_root_page_id = db->catalog->tables[i].root_page_id;
                break;
            }
        }

        if (table_root_page_id == 0) {
            fprintf(stderr, "Error: Table %s not found.\n", table_name);
            return NULL;
        }

        wal_log_insert(db->wal, db->current_tx_id, id, name);
        btree_insert(db->pool, db->pager, table_root_page_id, id, name, db->locked);
        return NULL;
    } else if (strncmp(query, "UPDATE", 6) == 0) {
        if (!db->locked) {
            fprintf(stderr, "Error: UPDATE statements must be within a transaction.\n");
            return NULL;
        }
        char table_name[MAX_NAME_LEN];
        int id;
        char name[256];
        sscanf(query, "UPDATE %s SET name = '%[^']' WHERE id = %d", table_name, name, &id);

        uint32_t table_root_page_id = 0;
        for (int i = 0; i < db->catalog->num_tables; i++) {
            if (strcmp(db->catalog->tables[i].table_name, table_name) == 0) {
                table_root_page_id = db->catalog->tables[i].root_page_id;
                break;
            }
        }

        if (table_root_page_id == 0) {
            fprintf(stderr, "Error: Table %s not found.\n", table_name);
            return NULL;
        }

        wal_log_update(db->wal, db->current_tx_id, id, name);
        btree_insert(db->pool, db->pager, table_root_page_id, id, name, db->locked);
        return NULL;
    } else if (strncmp(query, "DELETE FROM", 11) == 0) {
        if (!db->locked) {
            fprintf(stderr, "Error: DELETE statements must be within a transaction.\n");
            return NULL;
        }
        char table_name[MAX_NAME_LEN];
        int id;
        sscanf(query, "DELETE FROM %s WHERE id = %d", table_name, &id);

        uint32_t table_root_page_id = 0;
        for (int i = 0; i < db->catalog->num_tables; i++) {
            if (strcmp(db->catalog->tables[i].table_name, table_name) == 0) {
                table_root_page_id = db->catalog->tables[i].root_page_id;
                break;
            }
        }

        if (table_root_page_id == 0) {
            fprintf(stderr, "Error: Table %s not found.\n", table_name);
            return NULL;
        }

        wal_log_delete(db->wal, db->current_tx_id, id);
        btree_delete(db->pool, db->pager, table_root_page_id, id, db->locked);
        return NULL;
    } else if (strncmp(query, "SELECT", 6) == 0) {
        buffer_pool_flush_all(db->pool, db->pager);
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            db->pool->frames[i].page_id = -1;
        }

        char table_name[MAX_NAME_LEN];
        int id_present = 0;
        int id_value = 0;
        char op[4] = "";

        if (sscanf(query, "SELECT * FROM %s WHERE id %s %d", table_name, op, &id_value) == 3) {
            id_present = 1;
        } else if (sscanf(query, "SELECT * FROM %s", table_name) == 1) {
            // no where clause
        } else {
            fprintf(stderr, "Error: Invalid SELECT query format.\n");
            return NULL;
        }

        uint32_t table_root_page_id = 0;
        for (int i = 0; i < db->catalog->num_tables; i++) {
            if (strcmp(db->catalog->tables[i].table_name, table_name) == 0) {
                table_root_page_id = db->catalog->tables[i].root_page_id;
                break;
            }
        }

        if (table_root_page_id == 0) {
            fprintf(stderr, "Error: Table %s not found.\n", table_name);
            return NULL;
        }

        Result* result = (Result*)malloc(sizeof(Result));
        result->num_rows = 0;
        result->rows = NULL;

        // start from the leftmost leaf node (assumes root is leaf)
        uint32_t current_page_id = table_root_page_id;
        Page* current_page = buffer_pool_get_page(db->pool, db->pager, current_page_id);
        BTreeLeafNode* leaf_node = (BTreeLeafNode*)current_page;
        while (leaf_node != NULL) {
            for (int i = 0; i < leaf_node->header.num_keys; i++) {
                if (leaf_node->keys[i] == -1) continue; // skip deleted keys
                int matches_condition = 1;
                if (id_present) {
                    if (strcmp(op, "=") == 0) {
                        if (leaf_node->keys[i] != id_value) matches_condition = 0;
                    } else if (strcmp(op, "<") == 0) {
                        if (leaf_node->keys[i] >= id_value) matches_condition = 0;
                    } else if (strcmp(op, ">") == 0) {
                        if (leaf_node->keys[i] <= id_value) matches_condition = 0;
                    } else if (strcmp(op, "<=") == 0) {
                        if (leaf_node->keys[i] > id_value) matches_condition = 0;
                    } else if (strcmp(op, ">=") == 0) {
                        if (leaf_node->keys[i] < id_value) matches_condition = 0;
                    }
                }

                if (matches_condition) {
                    result->num_rows++;
                    result->rows = (char***)realloc(result->rows, sizeof(char**) * result->num_rows);
                    result->rows[result->num_rows - 1] = (char**)malloc(sizeof(char*) * 2);
                    result->rows[result->num_rows - 1][0] = (char*)malloc(10);
                    sprintf(result->rows[result->num_rows - 1][0], "%d", leaf_node->keys[i]);
                    result->rows[result->num_rows - 1][1] = (char*)malloc(strlen(leaf_node->values[i]) + 1);
                    strcpy(result->rows[result->num_rows - 1][1], leaf_node->values[i]);
                }
            }

            if (leaf_node->next_leaf != 0) {
                buffer_pool_unpin_page(db->pool, db->pager, current_page_id, 0);
                current_page_id = leaf_node->next_leaf;
                current_page = buffer_pool_get_page(db->pool, db->pager, current_page_id);
                leaf_node = (BTreeLeafNode*)current_page;
            } else {
                buffer_pool_unpin_page(db->pool, db->pager, current_page_id, 0);
                leaf_node = NULL;
            }
        }
        
        // return NULL if no rows were found
        if (result->num_rows == 0) {
            free(result);
            return NULL;
        }
        
        return result;
    }

    return NULL;
}

// Table printing utility implementation
TablePrinter* table_printer_create(const char* title, int num_columns) {
    if (num_columns <= 0) {
        return NULL;
    }
    
    TablePrinter* printer = (TablePrinter*)malloc(sizeof(TablePrinter));
    if (printer == NULL) {
        return NULL;
    }
    
    printer->headers = (char**)malloc(sizeof(char*) * num_columns);
    if (printer->headers == NULL) {
        free(printer);
        return NULL;
    }
    
    for (int i = 0; i < num_columns; i++) {
        printer->headers[i] = NULL;
    }
    
    printer->rows = NULL;
    printer->num_columns = num_columns;
    printer->num_rows = 0;
    
    if (title != NULL) {
        printer->title = (char*)malloc(strlen(title) + 1);
        if (printer->title != NULL) {
            strcpy(printer->title, title);
        }
    } else {
        printer->title = NULL;
    }
    
    return printer;
}

void table_printer_add_header(TablePrinter* printer, int col, const char* header) {
    if (printer == NULL || col < 0 || col >= printer->num_columns || header == NULL) {
        return;
    }
    
    if (printer->headers[col] != NULL) {
        free(printer->headers[col]);
    }
    
    printer->headers[col] = (char*)malloc(strlen(header) + 1);
    if (printer->headers[col] != NULL) {
        strcpy(printer->headers[col], header);
    }
}

void table_printer_add_row(TablePrinter* printer, char** row_data) {
    if (printer == NULL || row_data == NULL) {
        return;
    }
    
    // reallocate rows array
    printer->rows = (char***)realloc(printer->rows, sizeof(char**) * (printer->num_rows + 1));
    if (printer->rows == NULL) {
        return;
    }
    
    // allocate new row
    printer->rows[printer->num_rows] = (char**)malloc(sizeof(char*) * printer->num_columns);
    if (printer->rows[printer->num_rows] == NULL) {
        return;
    }
    
    // copy data to new row
    for (int i = 0; i < printer->num_columns; i++) {
        if (row_data[i] != NULL) {
            printer->rows[printer->num_rows][i] = (char*)malloc(strlen(row_data[i]) + 1);
            if (printer->rows[printer->num_rows][i] != NULL) {
                strcpy(printer->rows[printer->num_rows][i], row_data[i]);
            }
        } else {
            printer->rows[printer->num_rows][i] = (char*)malloc(1);
            if (printer->rows[printer->num_rows][i] != NULL) {
                printer->rows[printer->num_rows][i][0] = '\0';
            }
        }
    }
    
    printer->num_rows++;
}

void table_printer_print(TablePrinter* printer) {
    if (printer == NULL) {
        return;
    }
    
    // calculate column widths
    int* widths = (int*)malloc(sizeof(int) * printer->num_columns);
    if (widths == NULL) {
        return;
    }
    
    // initialize with header widths
    for (int i = 0; i < printer->num_columns; i++) {
        widths[i] = (printer->headers[i] != NULL) ? strlen(printer->headers[i]) : 0;
    }
    
    // check row data for max widths
    for (int r = 0; r < printer->num_rows; r++) {
        for (int c = 0; c < printer->num_columns; c++) {
            if (printer->rows[r][c] != NULL) {
                int len = strlen(printer->rows[r][c]);
                if (len > widths[c]) {
                    widths[c] = len;
                }
            }
        }
    }
    
    // ensure minimum width and add padding
    for (int i = 0; i < printer->num_columns; i++) {
        if (widths[i] < 6) {
            widths[i] = 6;
        }
        widths[i] += 2; // padding
    }
    
    // print title if provided
    if (printer->title != NULL) {
        // calculate total width for centering
        int total_width = 0;
        for (int i = 0; i < printer->num_columns; i++) {
            total_width += widths[i];
        }
        total_width += (printer->num_columns - 1); // separators
        
        int title_len = strlen(printer->title);
        int padding = (total_width - title_len) / 2;
        if (padding > 0) {
            printf("%*s%s\n", padding, "", printer->title);
        } else {
            printf("%s\n", printer->title);
        }
    }
    
    // print headers
    for (int i = 0; i < printer->num_columns; i++) {
        printf(" %-*s", widths[i] - 1, 
               (printer->headers[i] != NULL) ? printer->headers[i] : "");
        if (i < printer->num_columns - 1) {
            printf("|");
        }
    }
    printf("\n");
    
    // print separator line
    for (int i = 0; i < printer->num_columns; i++) {
        for (int j = 0; j < widths[i]; j++) {
            printf("-");
        }
        if (i < printer->num_columns - 1) {
            printf("+");
        }
    }
    printf("\n");
    
    // print data rows
    for (int r = 0; r < printer->num_rows; r++) {
        for (int c = 0; c < printer->num_columns; c++) {
            printf(" %-*s", widths[c] - 1,
                   (printer->rows[r][c] != NULL) ? printer->rows[r][c] : "");
            if (c < printer->num_columns - 1) {
                printf("|");
            }
        }
        printf("\n");
    }
    
    // print row count
    printf("(%d row%s)\n\n", printer->num_rows, printer->num_rows == 1 ? "" : "s");
    
    free(widths);
}

void table_printer_free(TablePrinter* printer) {
    if (printer == NULL) {
        return;
    }
    
    // free headers
    if (printer->headers != NULL) {
        for (int i = 0; i < printer->num_columns; i++) {
            if (printer->headers[i] != NULL) {
                free(printer->headers[i]);
            }
        }
        free(printer->headers);
    }
    
    // free rows
    if (printer->rows != NULL) {
        for (int r = 0; r < printer->num_rows; r++) {
            if (printer->rows[r] != NULL) {
                for (int c = 0; c < printer->num_columns; c++) {
                    if (printer->rows[r][c] != NULL) {
                        free(printer->rows[r][c]);
                    }
                }
                free(printer->rows[r]);
            }
        }
        free(printer->rows);
    }
    
    // free title
    if (printer->title != NULL) {
        free(printer->title);
    }
    
    free(printer);
}

void db_help(void) {
    printf("General\n");
    printf("  \\q                     quit database\n");
    printf("  \\?                     show this help\n");
    printf("  \\h                     show this help\n");
    printf("\n");
    printf("Informational\n");
    printf("  \\l                     list databases\n");
    printf("  \\d                     list tables, views, and sequences\n");
    printf("  \\dt                    list tables only\n");
    printf("  \\d NAME                describe table, view, or sequence\n");
    printf("\n");
}

void db_list_databases(const char* filename) {
    TablePrinter* printer = table_printer_create("List of databases", 6);
    if (printer == NULL) {
        printf("Error: Unable to create table printer.\n");
        return;
    }
    
    table_printer_add_header(printer, 0, "Name");
    table_printer_add_header(printer, 1, "Owner");
    table_printer_add_header(printer, 2, "Encoding");
    table_printer_add_header(printer, 3, "Collate");
    table_printer_add_header(printer, 4, "Ctype");
    table_printer_add_header(printer, 5, "Access privileges");
    
    char* row_data[6] = {
        (char*)filename,
        "user",
        "UTF8",
        "en_US.UTF-8",
        "en_US.UTF-8",
        ""
    };
    
    table_printer_add_row(printer, row_data);
    table_printer_print(printer);
    table_printer_free(printer);
}

void db_list_tables(Database* db) {
    if (db == NULL || db->catalog == NULL) {
        printf("No tables found.\n");
        return;
    }

    if (db->catalog->num_tables == 0) {
        printf("No tables found.\n");
        return;
    }
    
    TablePrinter* printer = table_printer_create("List of relations", 4);
    if (printer == NULL) {
        printf("Error: Unable to create table printer.\n");
        return;
    }
    
    table_printer_add_header(printer, 0, "Schema");
    table_printer_add_header(printer, 1, "Name");
    table_printer_add_header(printer, 2, "Type");
    table_printer_add_header(printer, 3, "Owner");
    
    for (int i = 0; i < db->catalog->num_tables; i++) {
        char* row_data[4] = {
            "public",
            db->catalog->tables[i].table_name,
            "table",
            "user"
        };
        table_printer_add_row(printer, row_data);
    }
    
    table_printer_print(printer);
    table_printer_free(printer);
}

void db_describe_table(Database* db, const char* table_name) {
    if (db == NULL || db->catalog == NULL || table_name == NULL) {
        printf("Error: Invalid parameters.\n");
        return;
    }

    TableSchema* table = NULL;
    for (int i = 0; i < db->catalog->num_tables; i++) {
        if (strcmp(db->catalog->tables[i].table_name, table_name) == 0) {
            table = &db->catalog->tables[i];
            break;
        }
    }

    if (table == NULL) {
        printf("Did not find any relation named \"%s\".\n", table_name);
        return;
    }

    char title[128];
    snprintf(title, sizeof(title), "Table \"public.%s\"", table->table_name);
    
    TablePrinter* printer = table_printer_create(title, 5);
    if (printer == NULL) {
        printf("Error: Unable to create table printer.\n");
        return;
    }
    
    table_printer_add_header(printer, 0, "Column");
    table_printer_add_header(printer, 1, "Type");
    table_printer_add_header(printer, 2, "Collation");
    table_printer_add_header(printer, 3, "Nullable");
    table_printer_add_header(printer, 4, "Default");
    
    for (int i = 0; i < table->num_columns; i++) {
        ColumnSchema* col = &table->columns[i];
        char type_str[64];
        
        if (col->type == COLUMN_TYPE_INT) {
            strcpy(type_str, "integer");
        } else if (col->type == COLUMN_TYPE_VARCHAR) {
            snprintf(type_str, sizeof(type_str), "character varying(%d)", col->length);
        } else {
            strcpy(type_str, "unknown");
        }
        
        char* row_data[5] = {
            col->name,
            type_str,
            "",
            "YES",
            ""
        };
        table_printer_add_row(printer, row_data);
    }
    
    table_printer_print(printer);
    table_printer_free(printer);
    
    // show indexes
    printf("Indexes:\n");
    for (int i = 0; i < table->num_columns; i++) {
        if (table->columns[i].is_primary_key) {
            printf("    \"%s_pkey\" PRIMARY KEY, btree (%s)\n", 
                   table->table_name, table->columns[i].name);
        }
    }
    printf("\n");
}

void db_describe_all(Database* db) {
    if (db == NULL || db->catalog == NULL) {
        printf("No relations found.\n");
        return;
    }

    if (db->catalog->num_tables == 0) {
        printf("No relations found.\n");
        return;
    }
    
    TablePrinter* printer = table_printer_create("List of relations", 6);
    if (printer == NULL) {
        printf("Error: Unable to create table printer.\n");
        return;
    }
    
    table_printer_add_header(printer, 0, "Schema");
    table_printer_add_header(printer, 1, "Name");
    table_printer_add_header(printer, 2, "Type");
    table_printer_add_header(printer, 3, "Owner");
    table_printer_add_header(printer, 4, "Size");
    table_printer_add_header(printer, 5, "Description");
    
    for (int i = 0; i < db->catalog->num_tables; i++) {
        char* row_data[6] = {
            "public",
            db->catalog->tables[i].table_name,
            "table",
            "user",
            "8192 bytes",
            ""
        };
        table_printer_add_row(printer, row_data);
    }
    
    table_printer_print(printer);
    table_printer_free(printer);
}