#!/bin/bash

BINARY="./kerneldb"
PASS=0
FAIL=0

cleanup() {
    rm -rf data
    mkdir -p data/testdb
}

run_clean() {
    printf "%s\n" "$@" | $BINARY > /tmp/kdb.txt 2>/dev/null
}

run_crash() {
    DELAY=$1
    shift

    printf "%s\n" "$@" | $BINARY > /tmp/kdb.txt 2>/dev/null &
    PID=$!
    sleep "$DELAY"
    kill -9 $PID 2>/dev/null
    wait $PID 2>/dev/null
}

pass() { echo "[✔ PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "[❌ FAIL] $1"; FAIL=$((FAIL+1)); }

echo "========================================"
echo "  WAL STRICT TEST SUITE"
echo "========================================"

# -------------------------------
# TEST 1: UNCOMMITTED MUST DIE
# -------------------------------
cleanup

run_clean "USE testdb" "CREATE TABLE t (id INT, val TEXT)" "EXIT"

run_crash 0.5 "USE testdb" "BEGIN" "INSERT INTO t VALUES (1, ghost)"

run_clean "USE testdb" "SELECT * FROM t" "EXIT"
RES=$(cat /tmp/kdb.txt)

if echo "$RES" | grep -q "ghost"; then
    fail "Uncommitted row survived"
else
    pass "Uncommitted row removed"
fi

# -------------------------------
# TEST 2: COMMITTED MUST SURVIVE
# -------------------------------
cleanup

run_clean "USE testdb" "CREATE TABLE t (id INT, val TEXT)" "EXIT"

run_crash 0.8 "USE testdb" "BEGIN" "INSERT INTO t VALUES (2, survive)" "COMMIT"

run_clean "USE testdb" "SELECT * FROM t" "EXIT"
RES=$(cat /tmp/kdb.txt)

if echo "$RES" | grep -q "survive"; then
    pass "Committed row recovered"
else
    fail "Committed row missing"
fi

# -------------------------------
# TEST 3: NO DUPLICATES
# -------------------------------
cleanup

run_clean "USE testdb" "CREATE TABLE t (id INT, val TEXT)" "EXIT"

run_crash 0.8 "USE testdb" "BEGIN" "INSERT INTO t VALUES (5, dup)" "COMMIT"

run_clean "USE testdb" "SELECT * FROM t" "EXIT"
COUNT=$(grep -c "dup" /tmp/kdb.txt)

if [ "$COUNT" -eq 1 ]; then
    pass "No duplicate after recovery"
else
    fail "Duplicate rows detected ($COUNT)"
fi

# -------------------------------
# SUMMARY
# -------------------------------
echo "========================================"
echo "PASS: $PASS | FAIL: $FAIL"