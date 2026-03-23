#!/bin/bash
# WAL stress test for KernelDB
# Inserts 20 rows then simulates a crash

echo "=== KernelDB WAL Stress Test ==="
echo ""

# Clean slate
rm -rf data/
mkdir -p data

# Build commands
CMDS="CREATE TABLE students (id INT, name TEXT, mark INT)"
for i in $(seq 1 20); do
    CMDS="$CMDS
INSERT INTO students VALUES ($i, student$i, $((RANDOM % 100)))"
done
CMDS="$CMDS
SELECT * FROM students
.exit"

echo "--- Step 1: Insert 20 rows ---"
echo "$CMDS" | ./kerneldb

echo ""
echo "--- Step 2: Simulate crash (kill after 5 inserts) ---"
# Insert 5 more rows then kill
{
    echo "INSERT INTO students VALUES (21, crash_test1, 99)"
    echo "INSERT INTO students VALUES (22, crash_test2, 88)"
    echo "INSERT INTO students VALUES (23, crash_test3, 77)"
    sleep 0.5
} | ./kerneldb &
PID=$!
sleep 0.3
kill -9 $PID 2>/dev/null
echo "Process killed (simulated crash)"

echo ""
echo "--- Step 3: Restart — WAL should recover all rows ---"
echo -e "SELECT * FROM students\n.exit" | ./kerneldb

echo ""
echo "=== Test Complete ==="