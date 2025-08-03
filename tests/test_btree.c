#include "../src/database.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_all_data_types() {
    Database* db = db_open("test_all_types.db");
    assert(db != NULL);

    const char* create_query = "CREATE TABLE test_types ("
                               "id INT PRIMARY KEY, "
                               "name VARCHAR(50), "
                               "score FLOAT, "
                               "rate DOUBLE, "
                               "description TEXT, "
                               "creation_date DATE, "
                               "last_updated TIMESTAMP, "
                               "is_active BOOLEAN)";
    db_execute(db, create_query);

    const char* insert_query = "INSERT INTO test_types VALUES (1, 'test_name', 99.9, 3.14159, 'this is a text field.', '2024-01-01', '2024-01-01 12:00:00', true)";
    db_execute(db, "BEGIN");
    db_execute(db, insert_query);
    db_execute(db, "COMMIT");

    Result* result = db_execute(db, "SELECT * FROM test_types");
    assert(result != NULL);
    assert(result->num_rows == 1);
    assert(strcmp(result->rows[0][0], "1") == 0);
    assert(strcmp(result->rows[0][1], "test_name") == 0);
    assert(strcmp(result->rows[0][2], "99.9") == 0);
    assert(strcmp(result->rows[0][3], "3.14159") == 0);
    assert(strcmp(result->rows[0][4], "this is a text field.") == 0);
    assert(strcmp(result->rows[0][5], "2024-01-01") == 0);
    assert(strcmp(result->rows[0][6], "2024-01-01 12:00:00") == 0);
    assert(strcmp(result->rows[0][7], "true") == 0);

    printf("All data types test passed.\n");

    db_close(db);
}

int main() {
    test_all_data_types();
    
    Database* db = db_open("test.db");
    assert(db != NULL);

    db_execute(db, "CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(255))");

    db_execute(db, "BEGIN");
    db_execute(db, "INSERT INTO users VALUES (1, 'Alice')");
    db_execute(db, "INSERT INTO users VALUES (2, 'Bob')");
    db_execute(db, "INSERT INTO users VALUES (3, 'Charlie')");
    db_execute(db, "INSERT INTO users VALUES (4, 'David')");
    db_execute(db, "COMMIT");

    Result* result = db_execute(db, "SELECT * FROM users WHERE id = 1");
    assert(result != NULL);
    assert(result->num_rows == 1);
    assert(strcmp(result->rows[0][1], "Alice") == 0);

    result = db_execute(db, "SELECT * FROM users");
    assert(result != NULL);
    assert(result->num_rows == 4);

    result = db_execute(db, "SELECT * FROM users WHERE id < 3");
    assert(result != NULL);
    assert(result->num_rows == 2);
    assert(strcmp(result->rows[0][1], "Alice") == 0);
    assert(strcmp(result->rows[1][1], "Bob") == 0);

    result = db_execute(db, "SELECT * FROM users WHERE id > 2");
    assert(result != NULL);
    assert(result->num_rows == 2);
    assert(strcmp(result->rows[0][1], "Charlie") == 0);
    assert(strcmp(result->rows[1][1], "David") == 0);

    result = db_execute(db, "SELECT * FROM users WHERE id <= 2");
    assert(result != NULL);
    assert(result->num_rows == 2);
    assert(strcmp(result->rows[0][1], "Alice") == 0);
    assert(strcmp(result->rows[1][1], "Bob") == 0);

    result = db_execute(db, "SELECT * FROM users WHERE id >= 3");
    assert(result != NULL);
    assert(result->num_rows == 2);
    assert(strcmp(result->rows[0][1], "Charlie") == 0);
    assert(strcmp(result->rows[1][1], "David") == 0);

    db_execute(db, "BEGIN");
    db_execute(db, "UPDATE users SET name = 'Alicia' WHERE id = 1");
    db_execute(db, "COMMIT");

    result = db_execute(db, "SELECT * FROM users WHERE id = 1");
    assert(result != NULL);
    assert(result->num_rows == 1);
    assert(strcmp(result->rows[0][1], "Alicia") == 0);

    db_execute(db, "BEGIN");
    db_execute(db, "DELETE FROM users WHERE id = 2");
    db_execute(db, "COMMIT");

    result = db_execute(db, "SELECT * FROM users WHERE id = 2");
    assert(result == NULL);

    db_execute(db, "BEGIN");
    db_execute(db, "INSERT INTO users VALUES (5, 'Eve')");
    db_execute(db, "ROLLBACK");

    result = db_execute(db, "SELECT * FROM users WHERE id = 5");
    assert(result == NULL);

    printf("B-tree and transaction tests passed.\n");

    db_close(db);

    return 0;
}
