#!/bin/bash
#
# 详细追踪系统调用，特别是内存分配相关的 brk/mmap
#

set -e

if [ "$#" -lt 1 ]; then
    echo "用法: $0 <program>"
    echo "示例: $0 ./heap_bench"
    exit 1
fi

PROGRAM=$1
OUTPUT="syscalls_$(basename $PROGRAM).log"

echo "======================================"
echo "系统调用追踪: $PROGRAM"
echo "======================================"
echo ""

# 1. 统计模式
echo "1. 系统调用统计（所有）"
echo "----------------------"
strace -c $PROGRAM 2>&1 | tail -n 30
echo ""

# 2. 只看内存相关
echo "2. 内存分配系统调用统计"
echo "-----------------------"
strace -c -e trace=brk,mmap,munmap,mprotect,madvise $PROGRAM 2>&1 | tail -n 20
echo ""

# 3. 详细追踪
echo "3. 详细追踪内存系统调用"
echo "----------------------"
echo "输出到: $OUTPUT"
strace -e trace=brk,mmap,munmap,mprotect -o $OUTPUT $PROGRAM
echo ""

# 4. 分析结果
echo "4. 追踪结果分析"
echo "---------------"
echo ""

echo "前20个系统调用:"
head -n 20 $OUTPUT
echo ""

echo "系统调用次数统计:"
echo "  brk:    $(grep -c 'brk(' $OUTPUT || echo 0)"
echo "  mmap:   $(grep -c 'mmap(' $OUTPUT || echo 0)"
echo "  munmap: $(grep -c 'munmap(' $OUTPUT || echo 0)"
echo ""

echo "brk 调用详情（扩展 program break）:"
grep "brk(" $OUTPUT | head -n 10
echo ""

echo "mmap 调用详情（匿名内存映射）:"
grep "mmap(" $OUTPUT | head -n 10
echo ""

echo "完整日志: $OUTPUT"
echo ""
