#!/bin/bash
#
# 快速启动脚本
#

set -e

PROJECT_DIR="/Users/weli/works/stack-vs-heap-benchmark"
cd "$PROJECT_DIR"

echo "======================================"
echo "栈 vs 堆 基准测试 - 快速启动"
echo "======================================"
echo ""

# 检查 Docker
if ! command -v docker &> /dev/null; then
    echo "错误: 未找到 Docker，请先安装 Docker"
    exit 1
fi

if ! command -v docker-compose &> /dev/null; then
    echo "错误: 未找到 docker-compose，请先安装"
    exit 1
fi

# 构建容器
echo "步骤 1: 构建 Docker 镜像..."
echo "------------------------------"
docker-compose build
echo ""

# 启动容器
echo "步骤 2: 启动容器..."
echo "-------------------"
docker-compose up -d
sleep 2
echo ""

# 运行测试
echo "步骤 3: 运行基准测试..."
echo "-----------------------"
echo ""

docker-compose exec -T benchmark bash -c "
    cd /workspace
    echo '编译程序...'
    make clean && make all
    echo ''
    echo '=== 快速测试：混合对比 ==='
    echo ''
    ./mixed_bench
    echo ''
    echo '=== 使用 perf 统计 (栈) ==='
    echo ''
    perf stat -e cycles,cache-misses,page-faults ./stack_bench 2>&1 | tail -n 15
    echo ''
    echo '=== 使用 perf 统计 (堆) ==='
    echo ''
    perf stat -e cycles,cache-misses,page-faults ./heap_bench 2>&1 | tail -n 15
    echo ''
    echo '=== 系统调用追踪 (堆) ==='
    echo ''
    strace -c -e trace=brk,mmap,munmap ./heap_bench 2>&1 | grep -A 10 '% time' || echo '查看系统调用...'
    echo ''
"

echo ""
echo "======================================"
echo "测试完成！"
echo "======================================"
echo ""
echo "进入容器进行更多测试:"
echo "  docker-compose exec benchmark /bin/bash"
echo ""
echo "在容器内可以运行:"
echo "  make run              # 运行所有基准测试"
echo "  make perf             # 使用 perf 分析"
echo "  make strace           # 追踪系统调用"
echo "  ./scripts/run_all_benchmarks.sh  # 完整测试套件"
echo ""
echo "停止容器:"
echo "  docker-compose down"
echo ""
echo "详细文档: README.md"
echo ""
