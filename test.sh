#!/bin/bash

DB_EXEC="./kerneldb"
DATA_DIR="data"

echo "========================================"
echo "   KernelDB Strict Validation Suite"
echo "========================================"

TOTAL=0
FAIL=0

run_test() {
    NAME=$1
    QUERY=$2
    EXPECT=$3

    TOTAL=$((TOTAL+1))

    echo "$QUERY" | $DB_EXEC > tmp.out 2>/dev/null

    if grep -q "$EXPECT" tmp.out; then
        echo "[✔ PASS] $NAME"
    else
        echo "[❌ FAIL] $NAME"
        echo "   Expected: $EXPECT"
        echo "   Got:"
        cat tmp.out
        FAIL=$((FAIL+1))
    fi
}

reset_db() {
    rm -rf $DATA_DIR
    mkdir -p $DATA_DIR
    rm -f tmp.out
}

# -------------------------------
# 1. DDL
# -------------------------------
echo "[DDL]"
reset_db

run_test "Create Table" \
"CREATE TABLE users (id INT, name TEXT);" \
"OK"

# -------------------------------
# 2. INSERT
# -------------------------------
echo "[INSERT]"

run_test "Insert One" \
"INSERT INTO users VALUES (1, 'Alice'); SELECT * FROM users;" \
"Alice"

run_test "Insert Many" \
"INSERT INTO users VALUES (2, 'Bob'); INSERT INTO users VALUES (3, 'Charlie'); SELECT * FROM users;" \
"Charlie"

# -------------------------------
# 3. UPDATE
# -------------------------------
echo "[UPDATE]"

run_test "Update Row" \
"UPDATE users SET name='Updated' WHERE id=1; SELECT * FROM users;" \
"Updated"

# -------------------------------
# 4. DELETE
# -------------------------------
echo "[DELETE]"

echo "DELETE FROM users WHERE id=2;" | $DB_EXEC > /dev/null 2>&1
echo "SELECT * FROM users;" | $DB_EXEC > tmp.out

if grep -q "Bob" tmp.out; then
    echo "[❌ FAIL] Delete"
    FAIL=$((FAIL+1))
else
    echo "[✔ PASS] Delete"
fi
TOTAL=$((TOTAL+1))

# -------------------------------
# 5. PERSISTENCE
# -------------------------------
echo "[PERSISTENCE]"

echo "INSERT INTO users VALUES (10, 'Persist');" | $DB_EXEC > /dev/null

echo "SELECT * FROM users;" | $DB_EXEC > tmp.out

if grep -q "Persist" tmp.out; then
    echo "[✔ PASS] Persistence"
else
    echo "[❌ FAIL] Persistence"
    FAIL=$((FAIL+1))
fi
TOTAL=$((TOTAL+1))

# -------------------------------
# 6. BULK INSERT (STRICT)
# -------------------------------
echo "[BULK INSERT]"

CMDS=""
for i in $(seq 100 200); do
    CMDS="$CMDS
INSERT INTO users VALUES ($i, 'bulk_$i');"
done

echo "$CMDS" | $DB_EXEC > /dev/null
echo "SELECT * FROM users;" | $DB_EXEC > tmp.out

COUNT=$(grep -c "bulk_" tmp.out)

if [ "$COUNT" -eq 101 ]; then
    echo "[✔ PASS] Bulk insert exact"
else
    echo "[❌ FAIL] Bulk insert mismatch ($COUNT != 101)"
    FAIL=$((FAIL+1))
fi
TOTAL=$((TOTAL+1))

# -------------------------------
# 7. CRASH TEST (STRICT)
# -------------------------------
echo "[CRASH TEST]"

(
    {
        for i in $(seq 300 350); do
            echo "INSERT INTO users VALUES ($i, 'crash_$i');"
            sleep 0.01
        done
    } | $DB_EXEC
) &

PID=$!

sleep 0.2

# kill safely
kill -9 $PID 2>/dev/null
wait $PID 2>/dev/null

sleep 1

echo "SELECT * FROM users;" | $DB_EXEC > tmp.out

COUNT=$(grep -c "crash_" tmp.out)

if [ "$COUNT" -gt 0 ]; then
    echo "[✔ PASS] Crash recovery partial (expected)"
else
    echo "[❌ FAIL] No rows recovered after crash"
    FAIL=$((FAIL+1))
fi
TOTAL=$((TOTAL+1))

# -------------------------------
# 8. DUPLICATE CHECK (IMPORTANT)
# -------------------------------
echo "[DUPLICATE CHECK]"

DUP_COUNT=$(sort tmp.out | uniq -d | wc -l)

if [ "$DUP_COUNT" -eq 0 ]; then
    echo "[✔ PASS] No duplicate rows"
else
    echo "[❌ FAIL] Duplicate rows detected"
    FAIL=$((FAIL+1))
fi
TOTAL=$((TOTAL+1))

# -------------------------------
# FINAL
# -------------------------------
echo "========================================"
echo "TOTAL: $TOTAL | FAILED: $FAIL"

if [ "$FAIL" -eq 0 ]; then
    echo "🎉 SYSTEM STABLE"
    exit 0
else
    echo "⚠ ISSUES DETECTED"
    exit 1
fi