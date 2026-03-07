#!/bin/bash
#
# 使用 perf 进行深度性能分析
#

set -e

if [ "$#" -lt 1 ]; then
    echo "用法: $0 <program> [perf_options]"
    echo "示例: $0 ./stack_bench"
    echo "      $0 ./heap_bench \"-e cache-misses,cache-references\""
    exit 1
fi

PROGRAM=$1
PERF_OPTS=${2:-"-e cycles,instructions,cache-misses,cache-references,page-faults"}

echo "======================================"
echo "Perf 深度分析: $PROGRAM"
echo "======================================"
echo ""

# 1. 基础统计
echo "1. 基础性能统计"
echo "---------------"
perf stat $PERF_OPTS $PROGRAM
echo ""

# 2. 记录性能数据
echo "2. 记录性能事件"
echo "---------------"
PERF_DATA="perf_$(basename $PROGRAM).data"
echo "记录到: $PERF_DATA"
perf record -o $PERF_DATA -g $PROGRAM
echo ""

# 3. 生成报告
echo "3. 性能热点报告"
echo "---------------"
perf report -i $PERF_DATA --stdio | head -n 100
echo ""

# 4. 注解反汇编
echo "4. 热点函数的汇编注解（top 3）"
echo "------------------------------"
perf annotate -i $PERF_DATA --stdio | head -n 200
echo ""

echo "完成！完整报告可用以下命令查看:"
echo "  perf report -i $PERF_DATA"
echo ""
