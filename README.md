# 🚀 KernelDB

> A production-grade database engine built from scratch in C — implementing storage, indexing, query execution, and system-level control without external dependencies.

KernelDB is a **CLI-based embedded database engine** that demonstrates real-world database internals, including **B+ Tree indexing, slotted page storage, LRU buffer management, and query execution pipelines**.

---

## ✨ Key Highlights

- ⚡ **Indexed Queries** — B+ Tree enables O(log n) lookups
- 💾 **Storage Engine** — Slotted pages with variable-length records
- 🧠 **Buffer Pool** — LRU caching with dirty page write-back
- 🧩 **Query Execution** — SQL → Intent → Execution pipeline
- 🖥️ **CLI Interface** — Interactive REPL for real-time queries
- 🔧 **Built in C** — No external database libraries

---

## 🧠 How It Works (End-to-End Flow)

``` text
User Query (CLI)
      ↓
Parser → Intent
      ↓
Dispatcher
      ↓
Table Layer
      ↓
[Index Lookup OR Full Scan]
      ↓
Buffer Pool → Page → Disk
      ↓
Result Output
🏗️ Architecture Overview

KernelDB is structured into modular layers, each representing a core database system concept.

🟢 Layer 1 — Control Plane
Signal-safe REPL using poll()
Non-blocking event loop
Background monitoring thread (pthread)
Graceful signal handling (SIGINT, SIGTERM)

👉 Demonstrates OS-level system design

🟢 Layer 2 — SQL Parsing
Hand-written lexer + recursive descent parser
Converts SQL → structured Intent
Supports:
SELECT, INSERT, UPDATE, DELETE
CREATE TABLE, DROP TABLE

👉 No libraries — built from first principles

🔄 Layer 3 — WAL (In Progress)
Write-Ahead Logging for durability
Crash recovery via log replay
fsync() before commit

👉 Target: ACID compliance

🟢 Layer 4 — B+ Tree Index
Per-table index
Node splitting + balanced tree growth
Leaf node chaining for range scans
Automatic index maintenance on insert/update/delete

👉 Enables fast indexed queries

🟢 Layer 5 — Storage Engine + Buffer Pool
Slotted page layout (4KB pages)
Variable-length row storage
LRU buffer pool (7 frames per table)
Dirty page tracking + write-back

👉 Simulates real database storage systems

🔄 Layer 6 — Concurrency (Planned)
Reader-writer locks
Async I/O (io_uring)
Multi-threaded execution

🖥️ Example CLI Session
kerneldb> CREATE TABLE users (id INT, name TEXT)

kerneldb> INSERT INTO users VALUES (1, rohith)
kerneldb> INSERT INTO users VALUES (2, admin)

kerneldb> SELECT * FROM users
[full scan]

kerneldb> SELECT * FROM users WHERE id = 1
[index lookup]

⚡ Indexed Query Example
SELECT * FROM users WHERE id = 1
Uses B+ Tree lookup
Avoids full table scan
Direct page access via RowLocation

📦 Build & Run
git clone https://github.com/rohiths0402/kerneldb.git
cd kerneldb
make
./kerneldb

📁 Project Structure
kerneldb/
├── main.c
├── Makefile
├── data/
└── src/
  ├── repl/        # CLI + control plane
  ├── parser/      # SQL parsing
  ├── dispatcher/  # execution routing
  ├── index/       # B+ Tree index
  ├── storage/     # pages + buffer + table
  ├── monitor/     # background thread
  └── common/      # shared typeses

🛠️ Tech Stack
Language: C (C17)
I/O: POSIX (poll, fsync)
Memory: Manual allocation (posix_memalign)
Concurrency: pthread
Build: Makefile

🎯 Why This Project?

Most developers use databases — few understand how they work internally.

KernelDB is built to deeply explore:

How queries are parsed and executed
How indexes improve performance
How storage is managed on disk
How memory caching improves speed
How systems handle failures and recovery

👉 Every component is implemented from scratch.

🚀 Roadmap
 WAL + crash recovery
 Concurrency control
 Async I/O (io_uring)
 Query optimizer improvements
 
👨‍💻 Author
Rohith S
GitHub: https://github.com/rohiths0402
LinkedIn: https://linkedin.com/in/rohiths0402
Portfolio: https://rohithsportfolio.vercel.app
```
