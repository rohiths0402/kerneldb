KernelDB
A production-grade embedded database engine built from scratch in C.
KernelDB is an ongoing systems programming project that implements core database internals — WAL-based crash recovery, B+Tree indexing, slotted page storage, and a signal-safe control plane — without relying on any external database libraries.

Current Status
LayerComponentStatusLayer 1Control Plane (REPL, event loop, dispatcher)✅ CompleteLayer 2Parser (libpg_query / stub INSERT + SELECT)🔄 In ProgressLayer 3WAL + Crash Recovery🔄 PlannedLayer 4B+Tree Index🔄 PlannedLayer 5Slotted Page Storage🔄 PlannedLayer 6Concurrency + Buffer Pool🔄 Planned

Architecture
KernelDB is designed in 6 layers, each independently demonstrating a core database concept:
Layer 1 — Control Plane ✅

Signal-safe REPL with event loop using poll()/select()
Meta-commands: .exit, .help
Background task support (heartbeat/timer)
Dispatcher for routing commands to execution layer
Demonstrates: OS-level mastery — signals, non-blocking I/O, process management

Layer 2 — Parser 🔄

Stub parser or libpg_query integration
Supports INSERT and SELECT statements
Maps AST → internal Intent struct
Demonstrates: Parsing awareness without full grammar implementation

Layer 3 — WAL + Crash Recovery 🔄

Binary WAL record format: LSN, TxID, type, payload
fsync() before commit flag for durability
Recovery path: replay committed records, skip incomplete transactions
Crash simulation: kill process mid-write → restart → full recovery
Demonstrates: ACID durability, crash recovery, ARIES-style redo/undo

Layer 4 — B+Tree Index 🔄

Node splitting and merging
Maps logical keys → LRIDs (Logical Record IDs)
Indirection table: LRID → page offset
Fast lookup vs linear scan benchmark
Demonstrates: Algorithmic depth, data structures, index design

Layer 5 — Slotted Page Storage 🔄

4KB pages with header + slot array
Variable-length record support
Slot array grows from top, data grows from bottom
Optional defragmentation
Demonstrates: Low-level memory layout, page management

Layer 6 — Concurrency + Buffer Pool 🔄

Readers-writer lock implementation
Buffer pool: hot pages kept in RAM
io_uring integration (planned)
Demonstrates: Concurrency primitives, I/O optimization


Build & Run
bash# Clone the repo
git clone https://github.com/rohiths0402/kerneldb.git
cd kerneldb

# Build
make

# Run
./kerneldb
Example Session
kerneldb> .help
  .help     Show this message
  .exit     Exit KernelDB
kerneldb> .exit
Bye.

Tech Stack

Language: C (C11)
I/O: poll(), select(), POSIX file I/O
Storage: mmap, fsync, block-aligned I/O
Concurrency: pthreads, mutexes, RW locks
Debugging: GDB, Valgrind, AddressSanitizer
Build: Makefile


Why KernelDB?
Most developers use databases. Few understand how they work inside.
KernelDB is built to deeply understand:

How databases guarantee durability through WAL
How B+Trees enable fast lookups at scale
How slotted pages manage variable-length records
How crash recovery works at the byte level

Every component is implemented from scratch — no shortcuts, no libraries doing the heavy lifting.

Related Work
This project was built alongside hands-on experience contributing to a PostgreSQL-based database engine at IITM Pravartak (IIT Madras), where I implemented WAL, crash recovery, buffer pool management, and B+Tree indexing in production C code.

Author
Rohith S

GitHub: @rohiths0402
LinkedIn: Rohith S
