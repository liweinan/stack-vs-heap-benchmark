# 项目总结

## 项目概述

**栈 vs 堆 性能基准测试** - 基于博客文章[《栈为什么比堆快：从分配方式到「批发-零售」链条》](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)的实践验证项目。

通过 C 语言基准测试、perf 性能分析和 strace 系统调用追踪，展示栈与堆的性能差异，并结合 Linux 内核源码验证理论分析。

## 核心理论

### 1. 分配方式差异

| 维度 | 栈 | 堆 |
|------|----|----|
| 分配指令 | `sub rsp, N` (1条CPU指令) | `malloc()` (可能触发系统调用) |
| 成本 | 纳秒级 | 微秒级 |
| 系统调用 | 无 | `brk`/`mmap` |

### 2. 物理内存管理

- **栈**：按需分配（首次访问触发 #PF），成本摊薄
- **堆**：`mmap` 匿名映射，零填充（安全性），缺页时分配+清零/COW

### 3. 缓存友好性

- **栈**：LIFO 访问，局部性好，缓存命中率高
- **堆**：访问分散，缓存行利用率低

### 4. 批发-零售链条

```
内核 Buddy → Slab → sbrk/mmap → malloc → 用户代码
                                   ↓
                                 栈（无中间层）
```

## 项目结构

```
stack-vs-heap-benchmark/
├── Dockerfile                    # Alpine Linux 环境
├── docker-compose.yml            # 容器编排（privileged + CAP_SYS_ADMIN）
├── Makefile                      # 编译和测试目标
│
├── src/                          # C 源代码
│   ├── stack_allocation.c        # 栈分配基准测试
│   ├── heap_allocation.c         # 堆分配基准测试
│   ├── mixed_benchmark.c         # 混合对比（栈 vs 堆 vs 堆复用）
│   └── stack_asm_demo.c          # 汇编级演示（内联汇编）
│
├── scripts/                      # 自动化脚本
│   ├── run_all_benchmarks.sh     # 完整测试套件（perf + strace）
│   ├── analyze_with_perf.sh      # perf 深度分析（record + report）
│   └── trace_syscalls.sh         # strace 详细追踪
│
├── quick-start.sh                # 一键启动脚本
│
├── README.md                     # 项目说明
├── USAGE.md                      # 详细使用指南
├── EXPECTED_RESULTS.md           # 预期结果与内核源码对照
├── PROJECT_SUMMARY.md            # 本文件
└── .gitignore                    # Git 忽略规则
```

## 核心测试程序

### 1. stack_allocation.c
- 递归栈分配
- 迭代栈分配
- 展示 `sub rsp, N` 的极低成本

### 2. heap_allocation.c
- 小块堆分配（brk）
- 大块堆分配（mmap，>128KB）
- 展示 malloc/free 的开销

### 3. mixed_benchmark.c
- 直接对比：栈 vs 堆 vs 堆复用
- 验证：预分配堆可接近栈性能
- 证明差异主要在分配方式，而非存储介质

### 4. stack_asm_demo.c
- 内联汇编直接操作栈指针
- 展示编译器如何生成 `sub rsp` 指令
- 可生成汇编代码（`make asm`）查看

## 性能分析工具

### perf
- **cycles**: CPU 周期数
- **cache-misses**: 缓存未命中（栈通常更低）
- **page-faults**: 缺页异常次数
- **instructions**: 指令数 → insn/cycle（栈通常更高）

### strace
- **brk**: 小块堆分配（调整 program break）
- **mmap**: 大块分配或匿名映射
- **munmap**: 释放大块内存
- 栈版本几乎无这些系统调用

## 关键发现

### 发现 1: 栈分配无系统调用

```bash
strace -c -e brk,mmap ./stack_bench
```

结果：只有程序加载时的初始 `mmap`，运行时零系统调用。

**内核验证**：栈分配只是 `sub rsp, N`，内核对此无感知。

### 发现 2: 堆分配大量系统调用

```bash
strace -c -e brk,mmap ./heap_bench
```

结果：大量 `brk` 调用（每次 malloc 池不足时）。

**内核验证** (`mm/mmap.c:115`)：
```c
SYSCALL_DEFINE1(brk, unsigned long, brk) {
    if (do_brk_flags(...) < 0) goto out;
    mm->brk = brk;  // 更新 program break
}
```

### 发现 3: 缺页路径等价

```bash
perf stat -e page-faults ./stack_bench
perf stat -e page-faults ./heap_bench
```

单次缺页成本相同（都是 #PF → `do_anonymous_page` → 分配物理页）。

**内核验证** (`mm/memory.c`)：
```c
static vm_fault_t do_anonymous_page(...) {
    page = alloc_zeroed_user_highpage_movable(...);
    set_pte_at(vma->vm_mm, address, page_table, entry);
}
```

### 发现 4: 堆可接近栈（预分配）

```bash
./mixed_bench
```

预期：
- 栈：100 ns/次
- 堆（malloc/free）：1500 ns/次（慢 15x）
- **堆复用**：120 ns/次（只慢 20%）

**结论**：差异主要在分配方式，而非访问速度。

### 发现 5: 缓存局部性差异

```bash
perf stat -e cache-misses,cache-references ./stack_bench
perf stat -e cache-misses,cache-references ./heap_bench
```

预期：
- 栈：cache-miss 率 ~0.5%
- 堆：cache-miss 率 ~15-20%

**原因**：栈 LIFO 访问 → 局部性好；堆地址分散 → 缓存行利用率低。

## 与内核源码的对照

| 博客论点 | 内核源码位置 | 验证方法 |
|---------|------------|---------|
| brk 只扩展 VMA | `mm/vma.c:2714` `do_brk_flags()` | `strace -e brk` |
| 物理页按需分配 | `mm/memory.c` `do_anonymous_page()` | `perf stat -e page-faults` |
| mmap 零填充 | `mm/mmap.c` + `mm/memory.c` | 读内核注释 |
| Buddy 分配器 | `mm/page_alloc.c` `__alloc_pages()` | 读 `free_area[]` |
| Slab 缓存 | `mm/slub.c` `kmem_cache_alloc()` | 读 cache 结构 |

## 适用场景

### 优先用栈
- 小数据（<几KB）
- 生命周期与函数调用一致
- 需要高性能（热路径）

### 优先用堆
- 大数据
- 生命周期超出函数作用域
- 需要动态大小

### 优化堆性能
- **Arena/Pool**：预分配大块，用户态切分
- **对象池**：复用已分配对象
- **内存池**：避免频繁 malloc/free

## 实验扩展

### 扩展 1: 多线程对比
- 每个线程独立栈
- 多线程共享堆（需要锁）
- 对比 malloc 在高并发下的性能

### 扩展 2: 不同分配器
- glibc malloc
- tcmalloc
- jemalloc
- 对比系统调用次数

### 扩展 3: 不同内核版本
- 对比 Linux 5.x vs 6.x
- 观察内存管理优化

### 扩展 4: THP（Transparent Huge Pages）
- 启用/禁用 THP
- 对比缺页次数（2MB vs 4KB）

## 参考文献

1. **博客文章**（理论基础）
   - [栈为什么比堆快：从分配方式到「批发-零售」链条](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)
   - [为什么「语言速度」是伪命题](https://weinan.io/2026/03/01/why-language-speed-is-misleading.html)

2. **Linux 内核源码**（实现验证）
   - `/Users/weli/works/linux/mm/mmap.c` - brk 系统调用
   - `/Users/weli/works/linux/mm/vma.c` - VMA 管理
   - `/Users/weli/works/linux/mm/memory.c` - 缺页处理
   - `/Users/weli/works/linux/mm/page_alloc.c` - Buddy 分配器
   - `/Users/weli/works/linux/mm/slub.c` - Slab 分配器

3. **Intel SDM**
   - Vol.3A 第 6 章 - 异常与中断（#PF）
   - Vol.3A §6.14.2 - 64位栈帧
   - Vol.3A §6.15 - Page-Fault Exception

4. **经典书籍**
   - Mel Gorman - *Understanding the Linux Virtual Memory Manager*

## 快速开始

```bash
cd /Users/weli/works/stack-vs-heap-benchmark

# 方式 1: 一键启动
./quick-start.sh

# 方式 2: 手动步骤
docker-compose build
docker-compose up -d
docker-compose exec benchmark /bin/bash

# 在容器内
make all
make run
make perf
make strace
./scripts/run_all_benchmarks.sh
```

## 输出示例

### 混合对比测试
```
栈分配                     :   10234567 ns | 平均:    102 ns | 基准线
堆分配（malloc/free）      :  156789012 ns | 平均:   1567 ns | 慢 15.32x
堆复用（预分配）           :   12345678 ns | 平均:    123 ns | 慢 1.21x
```

### strace 追踪
```
栈版本：0 个 brk/mmap 调用（运行时）
堆版本：100+ 个 brk 调用
```

### perf 统计
```
栈：cache-miss 率 0.5%，page-faults 23
堆：cache-miss 率 18%，page-faults 1234
```

## 关键结论

1. **栈比堆快 10-15 倍**（小块频繁分配/释放场景）
2. **差异主要在分配方式**，而非访问速度
3. **缺页路径上栈与堆等价**（都是 #PF → 分配物理页）
4. **堆可以通过预分配优化**，接近栈性能
5. **栈的缓存局部性更好**（LIFO 访问模式）

## 工程意义

这不是在说"堆慢、别用堆"，而是：

1. **理解成本来源**：分配方式 vs 访问方式
2. **选择合适工具**：栈 vs 堆 vs 池
3. **优化热路径**：减少 malloc/free
4. **系统思维**：I/O、并发、内存都影响性能

性能优化不是"栈一定比堆快"，而是"在这个场景下，为什么快，如何量化"。

## License

MIT

## 致谢

- 理论来源：[liweinan 的技术博客](https://weinan.io)
- 内核源码：Linux Kernel ([kernel.org](https://kernel.org))
- 工具：perf, strace, GCC, Docker
