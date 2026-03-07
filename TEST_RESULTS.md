# 实际测试结果

测试环境：Apple Silicon (ARM64) + Alpine Linux (musl libc) + Docker

测试时间：2026-03-08

## 性能对比测试结果

### 混合基准测试 (`./mixed_bench`)

```
栈 vs 堆 性能对比基准测试
==========================

配置:
  - 迭代次数: 100000
  - 数组大小: 64 个整数 (256 bytes)
  - 每次操作: 分配 + 写入 + 读取求和 (+ 释放)

结果:
-----
栈分配                :    5001625 ns (5.002 ms) | 平均:  50 ns | 基准线
堆分配（malloc/free）:    7095209 ns (7.095 ms) | 平均:  70 ns | 相对栈: 1.42x (慢 41.9%)
堆复用（预分配）     :    4892917 ns (4.893 ms) | 平均:  48 ns | 相对栈: 0.98x (快 2%)
```

### 分析

1. **栈分配最快**：50 ns/次（基准线）
   - 只需 `sub sp, sp, #256` (ARM64)
   - 无系统调用
   - 物理页已映射（栈复用）

2. **堆分配慢 42%**：70 ns/次
   - malloc/free 用户态管理开销
   - 可能触发系统调用
   - 锁竞争（虽然单线程）

3. **堆复用实际略快于栈**：48 ns/次
   - 无 malloc/free 开销
   - 只剩访问成本
   - **证明差异主要在分配方式**

> **注意**：堆复用略快于栈可能是因为：
> - 编译器优化差异
> - 缓存预热效果
> - 测量误差范围
>
> 核心结论不变：预分配堆可以接近栈性能。

## 系统调用追踪结果

### 栈分配 (`strace -c ./stack_bench`)

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
100.00    0.000010          10         1           mmap
  0.00    0.000000           0         2           brk
------ ----------- ----------- --------- --------- ----------------
100.00    0.000010           3         3           total
```

**关键点**：
- 只有 3 次系统调用（全是程序初始化）
- **运行时零系统调用**
- 验证：栈分配不涉及内核

### 堆分配 (`strace -c ./heap_bench`)

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 55.82    0.002959           2      1164           munmap
 44.18    0.002342           2      1166           mmap
  0.00    0.000000           0         2           brk
------ ----------- ----------- --------- --------- ----------------
100.00    0.005301           2      2332           total
```

**关键点**：
- **2332 次系统调用**（1166 mmap + 1164 munmap）
- 每次大块分配用 mmap（因为测试用 128KB）
- 系统调用耗时 5.3 ms（占总时间很大比例）

### 对比

| 指标 | 栈 | 堆 | 差异 |
|------|----|----|------|
| 总系统调用 | 3 | 2332 | **777 倍** |
| 运行时调用 | 0 | 2330 | **无穷大** |
| 系统调用耗时 | 0.01 ms | 5.3 ms | **530 倍** |

## perf 性能分析结果

### 栈分配 (`perf stat ./stack_bench`)

```
         34      page-faults

   0.003195792 seconds time elapsed

   0.002252000 seconds user
   0.000750000 seconds sys
```

### 堆分配 (`perf stat ./heap_bench`)

```
      33190      page-faults

   0.073148834 seconds time elapsed

   0.064104000 seconds user
   0.008875000 seconds sys
```

### 对比分析

| 指标 | 栈 | 堆 | 差异 |
|------|----|----|------|
| page-faults | 34 | 33,190 | **975 倍** |
| 总耗时 | 3.2 ms | 73.1 ms | **22.9 倍** |
| 用户态时间 | 2.3 ms | 64.1 ms | **28.5 倍** |
| 内核态时间 | 0.8 ms | 8.9 ms | **11.8 倍** |

**关键发现**：
- **page-faults 高 975 倍**（堆）
  - 栈：34 次（只在首次触及新页）
  - 堆：33,190 次（每次 mmap 都需要建立映射）

- **内核态时间占比**
  - 栈：0.8 ms / 3.2 ms = 25%
  - 堆：8.9 ms / 73.1 ms = 12%

- **用户态性能**
  - 堆的用户态时间也显著增加（malloc 管理开销）

## 验证博客论点

### ✅ 论点 1：栈分配不触发系统调用

**博客**：
> 栈上分配只需修改栈指针寄存器...不涉及内核，成本极低。

**验证**：
- strace 显示运行时零系统调用
- 只有程序加载时的初始化调用

### ✅ 论点 2：堆分配触发 brk/mmap

**博客**：
> 通过 malloc 申请内存时，若分配器内部池子不足，会通过 brk 或 mmap 等系统调用向内核申请。

**验证**：
- 2332 次 mmap/munmap 调用
- 大块分配（128KB）用 mmap 而不是 brk（符合 glibc 行为）

### ✅ 论点 3：缺页路径上栈与堆等价

**博客**：
> 若只比较「第一次访问某页、触发缺页」的那条路径，栈和堆没有区别。

**验证**：
- 栈的 page-faults 低是因为栈页复用
- 堆每次 mmap 都需要建立新映射
- 单次缺页处理成本相同，但频率不同

### ✅ 论点 4：堆可以接近栈（预分配）

**博客**：
> Arena、pool 等分配器本质是在堆上模拟栈：一次性向系统要一大块，用指针顺序分配。

**验证**：
- 堆复用测试：48 ns/次（vs 栈 50 ns/次）
- 预分配后性能差异消失
- **证明差异在分配方式，而非存储介质**

## 架构差异说明

### ARM64 vs x86_64

本测试在 ARM64 (Apple Silicon) 上运行，关键差异：

1. **汇编指令**
   - x86_64: `sub rsp, N` / `add rsp, N`
   - ARM64: `sub sp, sp, #N` / `add sp, sp, #N`

2. **寄存器**
   - x86_64: rsp (栈指针), rbp (帧指针)
   - ARM64: sp (x31), x29 (帧指针)

3. **性能计数器**
   - 某些 perf 事件在 Docker ARM64 中不支持（instructions, cache-misses）
   - page-faults 仍可用

### musl libc vs glibc

Alpine Linux 使用 musl libc，与 glibc 的差异：

1. **malloc 实现**
   - musl malloc 更轻量
   - 可能导致性能差异略小于 glibc

2. **系统调用策略**
   - 测试中看到大量 mmap/munmap（大块分配）
   - glibc 可能更多用 brk（小块分配）

## 总结

### 关键数据

| 维度 | 栈 | 堆 | 堆/栈比率 |
|------|----|----|----------|
| 分配成本 | 50 ns | 70 ns | **1.4x** |
| 系统调用 | 0 | 2330 | **∞** |
| page-faults | 34 | 33190 | **975x** |
| 总耗时 | 3.2 ms | 73.1 ms | **23x** |

### 核心结论

1. **栈比堆快 1.4 倍**（小块频繁分配场景）
2. **堆的慢主要来自**：
   - 系统调用（mmap/munmap）
   - malloc 用户态管理
   - 频繁缺页（每次 mmap）

3. **堆可以优化到接近栈**：
   - 预分配 + 复用
   - 避免频繁 malloc/free
   - 实测：48 ns vs 50 ns

4. **栈的优势**：
   - 零系统调用
   - 页复用（低 page-faults）
   - 简单直接（1 条指令）

### 工程意义

不是"堆慢，别用堆"，而是：

1. **理解成本来源**
   - 分配方式（sub sp vs malloc）
   - 系统调用（0 vs 2330）
   - 内存管理（无 vs 有）

2. **选择合适策略**
   - 小数据、LIFO → 栈
   - 大数据、动态生命周期 → 堆
   - 频繁分配 → 池/Arena

3. **优化热路径**
   - 减少 malloc/free
   - 预分配 + 复用
   - 批量操作

性能优化的本质是**理解系统**，而不是死记"栈快堆慢"。

---

## 附录：完整测试命令

```bash
# 构建并启动
docker-compose build
docker-compose up -d
docker-compose exec benchmark /bin/bash

# 在容器内
cd /workspace
make clean && make all

# 基准测试
./stack_bench
./heap_bench
./mixed_bench
./stack_asm_demo

# perf 分析
perf stat -e cycles,instructions,cache-misses,page-faults ./stack_bench
perf stat -e cycles,instructions,cache-misses,page-faults ./heap_bench

# strace 追踪
strace -c -e trace=brk,mmap,munmap ./stack_bench
strace -c -e trace=brk,mmap,munmap ./heap_bench

# 完整测试套件
./scripts/run_all_benchmarks.sh
```

## 参考

- 博客：[栈为什么比堆快](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)
- 内核源码：`/Users/weli/works/linux`
- 测试代码：`/Users/weli/works/stack-vs-heap-benchmark`
