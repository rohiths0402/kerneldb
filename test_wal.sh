#!/bin/bash

echo "=== KernelDB WAL Stress + Performance Test ==="
echo ""

# Clean state
rm -rf data/
mkdir -p data

# -----------------------------
# Step 1: Bulk insert (100 rows)
# -----------------------------
echo "--- Step 1: Bulk insert  ---"

CMDS="CREATE DATABASE testdb;
USE testdb;
CREATE TABLE users (id INT, name TEXT);"

for i in $(seq 1 10); do
    CMDS="$CMDS
INSERT INTO users VALUES ($i, 'user$i');"
done

CMDS="$CMDS
.exit"

START=$(date +%s%3N)

echo "$CMDS" | ./kerneldb > /dev/null

END=$(date +%s%3N)

echo "Inserted 100 rows in $((END-START)) ms"

# -----------------------------
# Step 2: Crash BEFORE commit
# -----------------------------
echo ""
echo "--- Step 2: Crash BEFORE COMMIT ---"

echo "
USE testdb;
BEGIN;
INSERT INTO users VALUES (101, 'crash_before');
" | ./kerneldb &

PID=$!
sleep 1
kill -9 $PID 2>/dev/null

echo "Process killed (before commit)"

# -----------------------------
# Step 3: Recovery check
# -----------------------------
echo ""
echo "--- Step 3: Recovery check (should NOT include 101) ---"

echo "
USE testdb;
SELECT * FROM users;
.exit
" | ./kerneldb

# -----------------------------
# Step 4: Crash AFTER commit
# -----------------------------
echo ""
echo "--- Step 4: Crash AFTER COMMIT ---"

echo "
USE testdb;
BEGIN;
INSERT INTO users VALUES (102, 'crash_after');
COMMIT;
" | ./kerneldb &

PID=$!
sleep 1
kill -9 $PID 2>/dev/null

echo "Process killed (after commit)"

# -----------------------------
# Step 5: Final recovery
# -----------------------------
echo ""
echo "--- Step 5: Final recovery (should include 102) ---"

echo "
USE testdb;
SELECT * FROM users;
.exit
" | ./kerneldb

echo ""
echo "=== Test Complete ==="