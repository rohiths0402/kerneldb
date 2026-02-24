Deep Layers (Full Demo)
Layer 1: Control Plane
- Signal‑safe REPL with event loop (poll()/select()).
- Meta‑commands (.exit, .help) + dispatcher.
- Background task demo (heartbeat/timer).
- Impact: Shows OS‑level mastery (signals, non‑blocking I/O).
Layer 3: WAL + Crash Recovery
- Binary WAL record (LSN, TxID, type, payload).
- fsync() before commit flag.
- Recovery path: replay committed records, skip incomplete.
- Crash simulation demo: kill process mid‑write → restart → recover.
- Impact: Proves durability and ACID awareness.
Layer 4: B+Tree Index
- Node splitting/merging (basic).
- Map logical keys → LRIDs.
- Indirection table for LRID → page offset.
- Demo: fast lookup vs linear scan.
- Impact: Shows algorithmic depth + data structures.
Layer 5: Slotted Pages
- 4KB page with header + slot array.
- Variable‑length records (strings).
- Grow slot array from top, data from bottom.
- Simple defragmentation (optional).
- Impact: Demonstrates low‑level memory management.

⚖️ Minimal Layers (Stub/Subset)
Layer 2: Parser
- Use libpg_query or stub parser.
- Support only INSERT and SELECT.
- Map AST → internal “Intent” struct.
- Impact: Shows parsing awareness without drowning in grammar.
Layer 6: Concurrency/I/O
- Simple readers‑writer lock demo.
- Buffer pool stub (keep hot pages in RAM).
- Mention io_uring in README, but don’t fully implement.
- Impact: Shows you understand concurrency, even if not production‑ready.
