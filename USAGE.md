# 使用指南

## 快速开始（推荐）

```bash
cd /Users/weli/works/stack-vs-heap-benchmark
./quick-start.sh
```

这会自动：
1. 构建 Docker 镜像
2. 启动容器
3. 编译程序
4. 运行快速测试
5. 显示 perf 和 strace 结果

## 手动步骤

### 1. 构建并启动容器

```bash
docker-compose build
docker-compose up -d
```

### 2. 进入容器

```bash
docker-compose exec benchmark /bin/bash
```

### 3. 编译程序

```bash
make clean
make all
```

### 4. 运行基准测试

#### 方式 1: 使用 Makefile 目标

```bash
# 运行所有基准测试
make run

# 使用 perf 分析
make perf

# 使用 strace 追踪系统调用
make strace

# 生成汇编代码
make asm
```

#### 方式 2: 直接运行程序

```bash
# 栈分配测试
./stack_bench

# 堆分配测试
./heap_bench

# 混合对比测试
./mixed_bench

# 汇编级演示
./stack_asm_demo
```

#### 方式 3: 运行完整测试套件

```bash
./scripts/run_all_benchmarks.sh
```

## 性能分析工具

### perf 分析

```bash
# 基础统计
perf stat -e cycles,instructions,cache-misses,page-faults ./mixed_bench

# 详细分析（需要脚本）
./scripts/analyze_with_perf.sh ./heap_bench

# 记录性能数据
perf record -g ./mixed_bench
perf report
```

### strace 系统调用追踪

```bash
# 统计模式
strace -c ./heap_bench

# 只看内存分配相关
strace -c -e trace=brk,mmap,munmap ./heap_bench

# 详细追踪
./scripts/trace_syscalls.sh ./heap_bench

# 保存到文件
strace -e trace=brk,mmap,munmap -o syscalls.log ./heap_bench
cat syscalls.log
```

### 查看汇编代码

```bash
# 生成汇编文件
make asm

# 查看栈分配的汇编
less stack_allocation.s

# 搜索关键指令
grep -A 5 "sub.*rsp" stack_allocation.s
grep -A 5 "call.*malloc" heap_allocation.s
```

## 理解输出

### 混合对比测试输出示例

```
栈 vs 堆 性能对比基准测试
==========================

配置:
  - 迭代次数: 100000
  - 数组大小: 64 个整数 (256 bytes)
  - 每次操作: 分配 + 写入 + 读取求和 (+ 释放)

结果:
-----
栈分配                     :   12345678 ns (12.346 ms) | 平均:    123 ns | 基准线
堆分配（malloc/free）      :  123456789 ns (123.457 ms) | 平均:   1234 ns | 相对栈: 10.00x (慢 900.0%)
堆复用（预分配）           :   15678901 ns (15.679 ms) | 平均:    156 ns | 相对栈: 1.27x (慢 27.0%)
```

**解读**：
- 栈分配最快（基准线）
- 堆分配慢约 10 倍（malloc/free 开销）
- 堆复用只慢 27%（主要是访问模式差异）

### perf 输出示例

```
Performance counter stats for './stack_bench':

        45,678,901      cycles
        98,765,432      instructions              #    2.16  insn per cycle
            12,345      cache-misses              #    1.23% of all cache refs
         1,001,234      cache-references
                45      page-faults

       0.015234567 seconds time elapsed
```

**关注指标**：
- `cache-misses`: 越低越好（栈通常更低）
- `page-faults`: 缺页次数（首次分配大块内存会触发）
- `insn per cycle`: 每周期指令数（越高越好）

### strace 输出示例

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 45.67    0.000123          12        10           brk
 23.45    0.000063          15         4           mmap
 12.34    0.000033          11         3           munmap
------ ----------- ----------- --------- --------- ----------------
100.00    0.000269                    17           total
```

**解读**：
- `brk`: 小块堆分配（调整 program break）
- `mmap`: 大块分配或初始堆区映射
- `munmap`: 释放大块内存

**对比**：
- 栈分配：几乎没有这些系统调用（只有程序加载时的初始化）
- 堆分配：大量 `brk`/`mmap` 调用

## 高级用法

### 1. 对比缓存性能

```bash
perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
    ./stack_bench

perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
    ./heap_bench
```

### 2. 查看内存映射

```bash
# 终端1: 运行长时间测试
./heap_bench &
PID=$!

# 终端2: 查看内存映射
cat /proc/$PID/maps | grep -E "(heap|stack)"
```

### 3. 使用 gdb 调试

```bash
gdb ./stack_bench
(gdb) break main
(gdb) run
(gdb) disassemble
(gdb) info registers
(gdb) x/16gx $rsp   # 查看栈内容
```

### 4. 对比汇编代码

```bash
# 生成汇编
make asm

# 对比栈分配（sub rsp）
grep -A 10 "iterative_stack_alloc:" stack_allocation.s

# 对比堆分配（call malloc）
grep -A 10 "iterative_heap_alloc:" heap_allocation.s
```

## 故障排查

### 容器无法启动

```bash
# 检查 Docker 状态
docker ps -a

# 查看日志
docker-compose logs

# 重新构建
docker-compose down
docker-compose build --no-cache
docker-compose up -d
```

### perf 报错 "Permission denied"

确保容器有 `CAP_SYS_ADMIN` 权限：

```yaml
# docker-compose.yml 已设置
privileged: true
cap_add:
  - SYS_ADMIN
```

### strace 报错 "Operation not permitted"

确保有 `CAP_SYS_PTRACE`：

```yaml
# docker-compose.yml 已设置
cap_add:
  - SYS_PTRACE
```

### 编译错误

```bash
# 确保在容器内
docker-compose exec benchmark /bin/bash

# 安装依赖
apk add --no-cache gcc musl-dev make

# 清理重新编译
make clean
make all
```

## 实验想法

### 实验 1: 改变数组大小

修改 `src/mixed_benchmark.c`:

```c
#define ARRAY_SIZE 1024  // 从 64 改为 1024
```

重新编译并运行，观察性能差异。

### 实验 2: 触发大块 mmap

修改 `src/heap_allocation.c`:

```c
const int LARGE_SIZE = 65536;  // 256KB，触发 mmap
```

使用 strace 观察是否用 `mmap` 而不是 `brk`。

### 实验 3: 内存池模拟

创建自己的内存池，对比性能：

```c
// 预分配池
char pool[1024 * 1024];  // 1MB
size_t pool_offset = 0;

void* pool_alloc(size_t size) {
    if (pool_offset + size > sizeof(pool)) return NULL;
    void *ptr = pool + pool_offset;
    pool_offset += size;
    return ptr;
}
```

### 实验 4: 多线程对比

使用 pthread 创建多线程，对比：
- 每个线程独立栈
- 多线程共享堆（需要锁）

## 参考资料

- 博客文章: https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html
- Linux 内核源码: /Users/weli/works/linux
- perf 文档: `man perf-stat`, `man perf-record`
- strace 文档: `man strace`

## 清理

```bash
# 停止容器
docker-compose down

# 删除镜像（可选）
docker-compose down --rmi all

# 清理编译产物
make clean
```
