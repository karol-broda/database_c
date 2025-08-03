# c_database

a simple database in c featuring b+ trees, transactions, and crash recovery.

## features

- b+ tree storage engine with ordered data access
- acid transactions with begin/commit/rollback support  
- write-ahead logging for crash recovery
- buffer pool with lru page replacement
- basic sql-like interface
- table schema management

## building

```bash
make
```

run tests:
```bash
make test
```

clean build artifacts:
```bash
make clean
```

## usage

start the database:
```bash
./bin/c_database mydb.db
```

example session:
```sql
db > \?
General
  \q                     quit database
  \?                     show this help
  \h                     show this help

Informational
  \l                     list databases
  \d                     list tables, views, and sequences
  \dt                    list tables only
  \d NAME                describe table, view, or sequence

db > CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50))
db > \dt
          List of relations
 Schema |   Name   | Type  |  Owner  
--------+----------+-------+---------
 public | users    | table | user   
(1 row)

db > \d users
                     Table "public.users"
 Column |       Type        | Collation | Nullable | Default 
--------+-------------------+-----------+----------+---------
 id     | integer           |           | YES      |        
 name   | character varying(50) |           | YES      |        
Indexes:
    "users_pkey" PRIMARY KEY, btree (id)

db > BEGIN
db > INSERT INTO users VALUES (1, 'alice')
db > INSERT INTO users VALUES (2, 'bob')
db > COMMIT
db > SELECT * FROM users
(1, alice)
(2, bob)
db > \q
```

## commands

### sql commands
- `CREATE TABLE name (columns)` - create a new table
- `BEGIN` - start transaction
- `COMMIT` - commit transaction
- `ROLLBACK` - rollback transaction
- `INSERT INTO table VALUES (...)` - insert row
- `UPDATE table SET col = value WHERE id = n` - update row
- `DELETE FROM table WHERE id = n` - delete row
- `SELECT * FROM table [WHERE id op value]` - query rows

### meta commands (postgres-style)
- `\q` - quit database
- `\?` or `\h` - show help
- `\l` - list databases
- `\d` - list all tables and relations
- `\dt` - list tables only  
- `\d tablename` - describe specific table structure

### other
- `.` - exit (alternative to `\q`)

## implementation notes

- pages are 4kb fixed size
- b+ tree order is 32
- buffer pool holds 100 pages
- simple table-level locking
- values limited to 256 chars
- built for learning, not production use

## license

public domain - do whatever you want with it