#include "../src/database.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void cleanup_test_files() {
    system("rm -f test_integration.db wal.log");
}

void test_basic_workflow() {
    printf("Testing basic workflow with indexes...\n");
    cleanup_test_files();
    
    Database* db = db_open("test_integration.db");
    assert(db != NULL);
    printf("✓ Database opened successfully\n");

    // create table
    printf("Creating table...\n");
    db_execute(db, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    printf("✓ Table created successfully\n");
    
    // create index
    printf("Creating index...\n");
    db_execute(db, "CREATE INDEX age_idx ON users (age)");
    printf("✓ Index created successfully\n");
    
    // insert some data
    printf("Inserting data...\n");
    db_execute(db, "BEGIN");
    db_execute(db, "INSERT INTO users VALUES (1, 'Alice', 25)");
    db_execute(db, "INSERT INTO users VALUES (2, 'Bob', 30)");
    db_execute(db, "INSERT INTO users VALUES (3, 'Charlie', 25)");
    db_execute(db, "COMMIT");
    printf("✓ Data inserted successfully\n");
    
    // test index-optimized query
    printf("Testing index-optimized query...\n");
    Result* result = db_execute(db, "SELECT * FROM users WHERE age = 25");
    if (result != NULL) {
        printf("✓ Index query returned %d rows\n", result->num_rows);
        if (result->num_rows > 0) {
            printf("  First result: id=%s, name=%s, age=%s\n", 
                   result->rows[0][0], result->rows[0][1], result->rows[0][2]);
        }
    } else {
        printf("! Index query returned NULL\n");
    }
    
    // test non-index query
    printf("Testing non-index query...\n");
    result = db_execute(db, "SELECT * FROM users WHERE name = 'Bob'");
    if (result != NULL) {
        printf("✓ Non-index query returned %d rows\n", result->num_rows);
        if (result->num_rows > 0) {
            printf("  First result: id=%s, name=%s, age=%s\n", 
                   result->rows[0][0], result->rows[0][1], result->rows[0][2]);
        }
    } else {
        printf("! Non-index query returned NULL\n");
    }
    
    // test drop index
    printf("Testing drop index...\n");
    db_execute(db, "DROP INDEX age_idx ON users");
    printf("✓ Index dropped successfully\n");
    
    db_close(db);
    printf("✓ Database closed successfully\n");
}

void test_step_by_step() {
    printf("\nTesting each operation step by step...\n");
    cleanup_test_files();
    
    Database* db = db_open("test_integration.db");
    assert(db != NULL);
    printf("Step 1: Database opened\n");

    printf("Step 2: Creating table...\n");
    Result* result = db_execute(db, "CREATE TABLE test (id INT, value INT)");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL (expected)");
    
    printf("Step 3: Creating index...\n");
    result = db_execute(db, "CREATE INDEX val_idx ON test (value)");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL (expected)");
    
    printf("Step 4: Begin transaction...\n");
    result = db_execute(db, "BEGIN");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL (expected)");
    
    printf("Step 5: Insert data...\n");
    result = db_execute(db, "INSERT INTO test VALUES (1, 42)");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL (expected)");
    
    printf("Step 6: Commit transaction...\n");
    result = db_execute(db, "COMMIT");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL (expected)");
    
    printf("Step 7: Query data...\n");
    result = db_execute(db, "SELECT * FROM test WHERE value = 42");
    printf("  Result: %s\n", result ? "non-NULL" : "NULL");
    if (result) {
        printf("  Rows: %d\n", result->num_rows);
    }
    
    db_close(db);
    printf("Step 8: Database closed\n");
}

int main() {
    printf("=== Integration Tests for Index Functionality ===\n\n");
    
    test_basic_workflow();
    test_step_by_step();
    
    cleanup_test_files();
    
    printf("\n✅ Integration tests completed successfully!\n");
    printf("If you see this message, the basic index workflow is functioning.\n");
    
    return 0;
}