#!/bin/bash
#
# 运行所有基准测试并收集结果
#

set -e

echo "======================================"
echo "栈 vs 堆 性能基准测试套件"
echo "======================================"
echo ""

# 1. 基础运行
echo "步骤 1/4: 运行基础基准测试"
echo "------------------------------------"
make run
echo ""

# 2. perf 分析
echo "步骤 2/4: 使用 perf 分析性能"
echo "------------------------------------"
echo ""
echo "[栈分配 - perf统计]"
echo "-------------------"
perf stat -e cycles,instructions,cache-misses,cache-references,page-faults,context-switches ./stack_bench 2>&1 | tail -n 20
echo ""

echo "[堆分配 - perf统计]"
echo "-------------------"
perf stat -e cycles,instructions,cache-misses,cache-references,page-faults,context-switches ./heap_bench 2>&1 | tail -n 20
echo ""

echo "[混合对比 - perf统计]"
echo "-------------------"
perf stat -e cycles,instructions,cache-misses,cache-references,page-faults,context-switches ./mixed_bench 2>&1 | tail -n 20
echo ""

# 3. strace 系统调用分析
echo "步骤 3/4: 使用 strace 追踪系统调用"
echo "------------------------------------"
echo ""
echo "[栈分配 - 系统调用统计]"
echo "----------------------"
strace -c -e trace=brk,mmap,munmap,mprotect ./stack_bench 2>&1 | grep -A 20 "% time"
echo ""

echo "[堆分配 - 系统调用统计]"
echo "----------------------"
strace -c -e trace=brk,mmap,munmap,mprotect ./heap_bench 2>&1 | grep -A 20 "% time"
echo ""

echo "[混合对比 - 系统调用统计]"
echo "------------------------"
strace -c -e trace=brk,mmap,munmap,mprotect ./mixed_bench 2>&1 | grep -A 20 "% time"
echo ""

# 4. 详细的 brk/mmap 追踪
echo "步骤 4/4: 详细追踪堆分配的系统调用"
echo "------------------------------------"
echo ""
echo "追踪 heap_bench 的 brk/mmap 调用..."
strace -e trace=brk,mmap,munmap -o heap_syscalls.log ./heap_bench > /dev/null 2>&1

echo "提取前50个系统调用:"
echo "-------------------"
head -n 50 heap_syscalls.log
echo ""
echo "(完整日志保存在 heap_syscalls.log)"
echo ""

# 统计系统调用次数
echo "系统调用统计:"
echo "-------------"
echo -n "brk 调用次数: "
grep -c "brk(" heap_syscalls.log || echo "0"
echo -n "mmap 调用次数: "
grep -c "mmap(" heap_syscalls.log || echo "0"
echo -n "munmap 调用次数: "
grep -c "munmap(" heap_syscalls.log || echo "0"
echo ""

echo "======================================"
echo "所有测试完成！"
echo "======================================"
echo ""
echo "关键结论参考博客: https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html"
echo ""
