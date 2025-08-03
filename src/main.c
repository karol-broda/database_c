#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dbfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }



    char* dbfile = argv[1];
    Database* db = db_open(dbfile);
    if (db == NULL) {
        fprintf(stderr, "Error opening database file.\n");
        exit(EXIT_FAILURE);
    }

    char query[256];
    while (1) {
        printf("db > ");
        fgets(query, 256, stdin);
        
        // remove newline character
        query[strcspn(query, "\n")] = 0;
        
        if (query[0] == '.') {
            break;
        }
        
        // handle PostgreSQL-like backslash commands
        if (query[0] == '\\') {
            if (strcmp(query, "\\q") == 0) {
                break;
            } else if (strcmp(query, "\\?") == 0 || strcmp(query, "\\h") == 0) {
                db_help();
                continue;
            } else if (strcmp(query, "\\l") == 0) {
                db_list_databases(dbfile);
                continue;
            } else if (strcmp(query, "\\d") == 0) {
                db_describe_all(db);
                continue;
            } else if (strcmp(query, "\\dt") == 0) {
                db_list_tables(db);
                continue;
            } else if (strncmp(query, "\\d ", 3) == 0) {
                char table_name[64];
                sscanf(query, "\\d %s", table_name);
                db_describe_table(db, table_name);
                continue;
            } else {
                printf("Invalid command \"%s\". Try \\? for help.\n", query);
                continue;
            }
        }

        Result* result = db_execute(db, query);
        if (result != NULL) {
            for (int i = 0; i < result->num_rows; i++) {
                printf("(%s, %s)\n", result->rows[i][0], result->rows[i][1]);
                free(result->rows[i][0]);
                free(result->rows[i][1]);
                free(result->rows[i]);
            }
            free(result->rows);
            free(result);
        }
    }

    db_close(db);
    return 0;
}
