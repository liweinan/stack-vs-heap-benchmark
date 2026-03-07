# 栈 vs 堆 性能基准测试

本项目展示栈分配与堆分配的性能差异，并通过 `perf` 和 `strace` 等工具分析底层原因。

## 理论背景

基于博客文章：[栈为什么比堆快：从分配方式到「批发-零售」链条](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)

### 核心要点

1. **分配方式差异**
   - **栈**：修改栈指针（`sub rsp, N`），一条CPU指令，纳秒级
   - **堆**：`malloc` 可能触发系统调用（`brk`/`mmap`），微秒级

2. **物理内存管理**
   - **栈**：按需分配（首次访问触发缺页 #PF），成本被摊薄
   - **堆**：`mmap` 匿名映射需零填充（安全性），缺页时分配+清零或COW

3. **缓存友好性**
   - **栈**：LIFO访问模式，局部性好，缓存命中率高
   - **堆**：访问分散，缓存行利用率低

4. **批发-零售链条**
   ```
   内核 Buddy → Slab → sbrk/mmap → malloc → 用户代码
                                        ↓
                                      栈（绕过中间层）
   ```

## 项目结构

```
.
├── Dockerfile              # Alpine Linux 环境
├── docker-compose.yml      # 容器编排
├── Makefile               # 编译和测试目标
├── src/
│   ├── stack_allocation.c  # 栈分配基准测试
│   ├── heap_allocation.c   # 堆分配基准测试
│   └── mixed_benchmark.c   # 混合对比测试
├── scripts/
│   ├── run_all_benchmarks.sh   # 运行所有测试
│   ├── analyze_with_perf.sh    # perf 深度分析
│   └── trace_syscalls.sh       # strace 系统调用追踪
└── README.md
```

## 快速开始

### 1. 构建容器

```bash
cd /Users/weli/works/stack-vs-heap-benchmark
docker-compose build
```

### 2. 启动容器

```bash
docker-compose up -d
docker-compose exec benchmark /bin/bash
```

### 3. 运行基准测试

在容器内执行：

```bash
# 编译所有程序
make all

# 运行所有基准测试
make run

# 使用 perf 分析
make perf

# 使用 strace 追踪系统调用
make strace
```

## 详细测试

### 测试 1：基础性能对比

```bash
# 运行混合对比测试
./mixed_bench
```

**预期结果**：
- 栈分配最快（基准线）
- 堆分配慢 2-10 倍（取决于 malloc 实现）
- 堆复用（预分配）接近栈速度

### 测试 2：perf 性能分析

```bash
# 对比 cache-misses
perf stat -e cache-misses,cache-references,page-faults ./stack_bench
perf stat -e cache-misses,cache-references,page-faults ./heap_bench
```

**关注指标**：
- `cache-misses`: 缓存未命中次数（栈应更少）
- `page-faults`: 缺页异常次数
- `cycles`: CPU 周期数

### 测试 3：系统调用追踪

```bash
# 追踪内存分配系统调用
strace -c -e trace=brk,mmap,munmap ./stack_bench
strace -c -e trace=brk,mmap,munmap ./heap_bench
```

**预期结果**：
- **栈**：几乎没有 `brk`/`mmap` 调用（只有程序初始化）
- **堆**：大量 `brk`（小块）或 `mmap`（大块，>128KB）调用

### 测试 4：详细系统调用日志

```bash
# 查看堆分配的 brk 调用细节
strace -e trace=brk,mmap ./heap_bench 2>&1 | grep -E "(brk|mmap)"
```

**观察点**：
- `brk(0)`: 查询当前 program break
- `brk(0x...)`: 扩展堆顶（sbrk 的底层实现）
- `mmap(..., MAP_ANONYMOUS, ...)`: 大块匿名内存分配

## 运行自动化测试套件

```bash
# 在容器内运行完整测试套件
./scripts/run_all_benchmarks.sh
```

输出包括：
1. 基础性能对比
2. perf 统计（cycles、cache-misses、page-faults）
3. strace 系统调用统计
4. 详细的 brk/mmap 追踪日志

## 核心代码解析

### 栈分配 (`src/stack_allocation.c`)

```c
void iterative_stack_alloc(int iterations, int size) {
    for (int iter = 0; iter < iterations; iter++) {
        int local_array[size];  // sub rsp, size*4 - 极快！

        // 首次访问触发缺页（如果页未映射）
        for (int i = 0; i < size; i++) {
            local_array[i] = i;  // 可能触发 #PF
        }
    }
}
```

**汇编对应**（简化）：
```asm
; 分配栈空间
sub    rsp, 0x400     ; 假设 size=256，256*4=1024=0x400

; 访问栈空间
mov    DWORD PTR [rsp], 0
mov    DWORD PTR [rsp+4], 1
; ...

; 函数返回时自动回收
add    rsp, 0x400
ret
```

### 堆分配 (`src/heap_allocation.c`)

```c
void iterative_heap_alloc(int iterations, int size) {
    for (int iter = 0; iter < iterations; iter++) {
        int *arr = malloc(size * sizeof(int));  // 可能触发 brk/mmap

        // 初始化（触发物理页分配 + 零填充）
        for (int i = 0; i < size; i++) {
            arr[i] = i;
        }

        free(arr);  // 可能触发 munmap（大块）
    }
}
```

**系统调用链**：
```
malloc() → 内部池不足 → brk()/mmap() → 内核分配页 → 返回地址
```

## 内核源码对照

基于 Linux 内核源码 (`/Users/weli/works/linux`)：

### 1. brk 系统调用 (`mm/mmap.c`)

```c
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
    // ...
    if (do_brk_flags(&vmi, brkvma, oldbrk, newbrk - oldbrk, 0) < 0)
        goto out;
    mm->brk = brk;  // 更新 program break

    // 只在 VM_LOCKED 时预填页
    if (mm->def_flags & VM_LOCKED)
        populate = true;
    // ...
    if (populate)
        mm_populate(oldbrk, newbrk - oldbrk);  // 预分配物理页
}
```

**关键点**：
- 默认只扩展 VMA（虚拟地址），不分配物理页
- 物理页在首次访问时由缺页处理分配

### 2. do_brk_flags (`mm/vma.c`)

```c
int do_brk_flags(...)
{
    vma = vm_area_alloc(mm);  // 只分配 VMA 结构
    vma_set_anonymous(vma);
    vma_set_range(vma, addr, addr + len, ...);
    // 无 alloc_pages，无 mm_populate
}
```

### 3. 缺页处理 (`mm/memory.c`)

```c
// 首次访问新栈/堆区域时
do_page_fault() → handle_pte_fault() → do_anonymous_page()
{
    // 分配物理页
    page = alloc_zeroed_user_highpage_movable(...);
    // 建立页表映射
    set_pte_at(mm, address, page_table, entry);
}
```

## 验证博客中的论点

### 论点 1：栈分配不触发系统调用

```bash
strace -e trace=brk,mmap ./stack_bench
```

**结果**：只有程序加载时的初始 `mmap`，运行时没有内存分配系统调用。

### 论点 2：堆分配触发 brk/mmap

```bash
strace -c -e trace=brk,mmap ./heap_bench
```

**结果**：大量 `brk` 调用（小块）或 `mmap` 调用（大块）。

### 论点 3：缺页路径上栈与堆等价

使用 `perf` 对比初次分配大数组：

```bash
perf stat -e page-faults ./stack_bench
perf stat -e page-faults ./heap_bench
```

**结果**：两者的 page-faults 数量接近（取决于数组大小）。

### 论点 4：栈的缓存友好性

```bash
perf stat -e cache-misses,cache-references ./stack_bench
perf stat -e cache-misses,cache-references ./heap_bench
```

**结果**：栈的 cache-miss 率通常更低（LIFO 访问模式）。

## 高级分析

### 1. 查看汇编代码

```bash
# 编译时保留汇编
gcc -S -O2 src/stack_allocation.c -o stack.s
cat stack.s | grep -A 10 "sub.*rsp"
```

### 2. 使用 perf record 热点分析

```bash
perf record -g ./mixed_bench
perf report
```

找到热点函数：
- `malloc` / `free` 在堆版本中占比高
- 栈版本几乎没有函数调用开销

### 3. 内存映射查看

```bash
# 运行程序时查看 /proc/[pid]/maps
./heap_bench &
PID=$!
cat /proc/$PID/maps | grep -E "(heap|stack)"
```

观察：
- `[heap]`: brk 扩展的区域
- `[stack]`: 栈区域
- 匿名映射：`mmap` 分配的大块

## 关键结论

根据测试结果和内核源码：

1. **栈分配快的原因**：
   - 只需修改栈指针（1 条指令）
   - 无系统调用，无用户态/内核态切换
   - 缺页摊薄（每页只触发一次）

2. **堆分配慢的原因**：
   - `malloc` 用户态管理开销（锁、查找、碎片）
   - 池不足时触发系统调用（`brk`/`mmap`）
   - 大块分配每次都可能 `mmap`+`munmap`

3. **缺页路径等价**：
   - 单次缺页处理，栈与堆成本相同
   - 差异在于分配虚拟空间的成本

4. **堆可以接近栈**：
   - 预分配+复用（Arena/Pool）
   - 避免频繁 `malloc`/`free`
   - 性能可接近栈（见 `mixed_bench` 的 heap_reuse）

## 参考文献

1. [栈为什么比堆快：从分配方式到「批发-零售」链条](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)
2. Linux 内核源码: `/Users/weli/works/linux`
   - `mm/mmap.c`: `sys_brk` 实现
   - `mm/vma.c`: `do_brk_flags`
   - `mm/memory.c`: 缺页处理
3. Intel SDM Vol.3A 第 6 章: 异常与中断处理
4. Mel Gorman: *Understanding the Linux Virtual Memory Manager*

## 故障排查

### 容器内无法运行 perf

```bash
# 确保容器有 CAP_SYS_ADMIN 权限
docker-compose down
docker-compose up -d
```

`docker-compose.yml` 已设置 `privileged: true` 和 `cap_add: [SYS_ADMIN]`。

### strace 显示 "Operation not permitted"

```bash
# 确保有 CAP_SYS_PTRACE
docker-compose exec benchmark sh -c 'cat /proc/self/status | grep Cap'
```

## License

MIT

## 作者

基于 [liweinan](https://weinan.io) 的博客文章实现。
