#!/bin/bash
# tests/run_all_tests.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Master Test Suite - Full Run       ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"

TOTAL=0
PASSED=0
FAILED=0

run_test() {
    local test_name=$1
    local test_path=$2
    
    echo -e "\n${YELLOW}🧪 Running: ${test_name}${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    if ${test_path}; then
        echo -e "${GREEN}✅ ${test_name}: PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}❌ ${test_name}: FAILED${NC}"
        FAILED=$((FAILED + 1))
    fi
    TOTAL=$((TOTAL + 1))
}

# Unit tests
run_test "Initialization" "./criterion/unit/test_initialization"
run_test "Query Management" "./criterion/unit/test_query_management"
run_test "Worker Management" "./criterion/unit/test_worker_management"
run_test "Cleanup" "./criterion/unit/test_cleanup"

run_test "Disconnection" "./criterion/disconnection/test_disconnection"

# Scheduler tests
run_test "Scheduler FIFO" "./criterion/scheduler/test_scheduler_fifo"
run_test "Scheduler PRIORITY" "./criterion/scheduler/test_scheduler_priority"
run_test "Aging" "./criterion/scheduler/test_aging"

# Concurrency tests
run_test "Simple aging" "./criterion/concurrency/test_aging_simple"
run_test "Aging concurrent" "./criterion/concurrency/test_aging_concurrent"
run_test "Deadlock detection" "./criterion/concurrency/test_deadlock_detection"
run_test "Scheduler integration" "./criterion/concurrency/test_scheduler_integration"

# Resumen
echo -e "\n${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║            Test Summary                ║${NC}"
echo -e "${BLUE}╠════════════════════════════════════════╣${NC}"
echo -e "${BLUE}║${NC} Total:  ${TOTAL}                             ${BLUE}║${NC}"
echo -e "${BLUE}║${NC} Passed: ${GREEN}${PASSED}${NC}                             ${BLUE}║${NC}"
echo -e "${BLUE}║${NC} Failed: ${RED}${FAILED}${NC}                              ${BLUE}║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"

if [ ${FAILED} -eq 0 ]; then
    echo -e "\n${GREEN}🎉 All tests passed!${NC}\n"
    exit 0
else
    echo -e "\n${RED}⚠️  Some tests failed!${NC}\n"
    exit 1
fi