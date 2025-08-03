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

db > CREATE TABLE products (
       id INT PRIMARY KEY, 
       name VARCHAR(100),
       price FLOAT,
       rating DOUBLE,
       description TEXT,
       created_date DATE,
       last_updated TIMESTAMP,
       is_available BOOLEAN
     )
db > \dt
          List of relations
 Schema |   Name     | Type  |  Owner  
--------+------------+-------+---------
 public | products   | table | user   
(1 row)

db > \d products
                     Table "public.products"
   Column    |         Type          | Collation | Nullable | Default 
-------------+-----------------------+-----------+----------+---------
 id          | integer               |           | YES      |        
 name        | character varying(100)|           | YES      |        
 price       | real                  |           | YES      |        
 rating      | double precision      |           | YES      |        
 description | text                  |           | YES      |        
 created_date| date                  |           | YES      |        
 last_updated| timestamp             |           | YES      |        
 is_available| boolean               |           | YES      |        
Indexes:
    "products_pkey" PRIMARY KEY, btree (id)

db > BEGIN
db > INSERT INTO products VALUES (1, 'laptop', 999.99, 4.5, 'high performance laptop', '2024-01-01', '2024-01-01 12:00:00', true)
db > INSERT INTO products VALUES (2, 'mouse', 29.99, 4.2, 'wireless mouse', '2024-01-02', '2024-01-02 10:30:00', true)
db > COMMIT
db > SELECT * FROM products WHERE price < 100
(2, mouse, 29.99, 4.2, wireless mouse, 2024-01-02, 2024-01-02 10:30:00, true)
db > \q
```

## supported data types

| data type | description | example values |
|-----------|-------------|----------------|
| `INT` | 32-bit signed integer | `42`, `-100`, `0` |
| `VARCHAR(n)` | variable-length string up to n chars | `'hello'`, `'user@email.com'` |
| `FLOAT` | single-precision floating point | `3.14`, `99.9`, `-1.5` |
| `DOUBLE` | double-precision floating point | `3.141592653589793`, `1e-10` |
| `TEXT` | variable-length text (no size limit) | `'long description here...'` |
| `DATE` | date values | `'2024-01-01'`, `'1999-12-31'` |
| `TIMESTAMP` | date and time values | `'2024-01-01 12:30:45'` |
| `BOOLEAN` | true/false values | `true`, `false` |

example table with all types:
```sql
CREATE TABLE example (
    id INT PRIMARY KEY,
    name VARCHAR(50),
    price FLOAT,
    precision_val DOUBLE,
    notes TEXT,
    birth_date DATE,
    created_at TIMESTAMP,
    is_active BOOLEAN
)
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