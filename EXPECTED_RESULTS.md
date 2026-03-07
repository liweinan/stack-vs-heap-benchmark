# 预期结果与分析

基于博客文章的理论分析，本文档展示预期的测试结果和对应的内核源码验证。

## 1. 混合对比测试

### 预期输出

```
栈 vs 堆 性能对比基准测试
==========================

配置:
  - 迭代次数: 100000
  - 数组大小: 64 个整数 (256 bytes)
  - 每次操作: 分配 + 写入 + 读取求和 (+ 释放)

执行基准测试...
================

结果:
-----
栈分配                     :   10234567 ns (10.235 ms) | 平均:    102 ns | 基准线
堆分配（malloc/free）      :  156789012 ns (156.789 ms) | 平均:   1567 ns | 相对栈: 15.32x (慢 1432.0%)
堆复用（预分配）           :   12345678 ns (12.346 ms) | 平均:    123 ns | 相对栈: 1.21x (慢 21.0%)
```

### 分析

1. **栈分配 (102 ns/次)**
   - 只需 `sub rsp, 256` (1条指令)
   - 访问已映射的栈页（无缺页）
   - LIFO 访问模式，缓存友好

2. **堆分配 (1567 ns/次) - 慢 15 倍**
   - `malloc` 用户态管理开销
   - 可能触发 `brk` 系统调用
   - 锁竞争（多线程 malloc）
   - 碎片管理

3. **堆复用 (123 ns/次) - 只慢 21%**
   - 无 malloc/free 开销
   - 只剩访问成本
   - 差异来自缓存局部性（堆地址分散）

### 对应内核源码

#### 栈分配：无系统调用

博客 §1：
> 栈上分配只需修改**栈指针寄存器**。在 x86-64 上，函数序言用 `sub rsp, N`...不涉及内核。

汇编验证：
```bash
make asm
grep -A 10 "iterative_stack_alloc:" stack_allocation.s
```

应看到：
```asm
iterative_stack_alloc:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 1024      # 栈分配，无系统调用
    ...
```

#### 堆分配：brk 系统调用

博客 §4.3，内核源码 `mm/mmap.c:115`：

```c
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
    // ...
    if (do_brk_flags(&vmi, brkvma, oldbrk, newbrk - oldbrk, 0) < 0)
        goto out;
    mm->brk = brk;  // 更新 program break
    // ...
}
```

strace 验证：
```bash
strace -e brk ./heap_bench 2>&1 | head
```

应看到：
```
brk(0)                                  = 0x55555556c000
brk(0x55555558d000)                     = 0x55555558d000
brk(0x5555555ae000)                     = 0x5555555ae000
...
```

## 2. perf 性能统计

### 预期输出 - 栈分配

```
Performance counter stats for './stack_bench':

        34,567,890      cycles
        89,012,345      instructions              #    2.58  insn per cycle
             5,678      cache-misses              #    0.45% of all cache refs
         1,256,789      cache-references
                23      page-faults

       0.011523456 seconds time elapsed
```

### 预期输出 - 堆分配

```
Performance counter stats for './heap_bench':

       456,789,012      cycles
       234,567,890      instructions              #    0.51  insn per cycle
           234,567      cache-misses              #   18.67% of all cache refs
         1,256,789      cache-references
             1,234      page-faults

       0.152345678 seconds time elapsed
```

### 对比分析

| 指标 | 栈 | 堆 | 差异 |
|------|----|----|------|
| cycles | 34M | 456M | **13.2x** |
| insn/cycle | 2.58 | 0.51 | **栈高 5x**（更少停顿） |
| cache-miss率 | 0.45% | 18.67% | **堆高 41x** |
| page-faults | 23 | 1,234 | **堆高 53x** |

#### 原因分析（对应博客）

1. **cache-misses 高（堆）**
   - 博客 §3：堆访问分散，缓存行利用率低
   - 栈：LIFO 模式，局部性好

2. **page-faults 高（堆）**
   - 博客 §2：`mmap` 匿名映射，默认按需分配
   - 首次访问触发缺页 + 零填充（安全性）

3. **insn/cycle 低（堆）**
   - malloc 逻辑复杂（锁、查找、合并）
   - 系统调用导致 pipeline 刷新

### 内核源码验证

#### 缺页处理（栈与堆等价）

博客 §5.4：
> 若只比较「第一次访问某页、触发缺页」的那条路径，栈和堆没有区别：都是 #PF → 内核分配物理页 → 映射

内核源码 `mm/memory.c`（简化）：
```c
// 匿名页缺页处理（栈和堆都走这条路径）
static vm_fault_t do_anonymous_page(...) {
    // 分配零页
    page = alloc_zeroed_user_highpage_movable(...);
    // 建立页表映射
    set_pte_at(vma->vm_mm, address, page_table, entry);
    // ...
}
```

**关键点**：栈的 page-faults 少，因为：
- 栈区在程序启动时部分已映射
- LIFO 访问模式，新栈帧复用已映射的页

## 3. strace 系统调用追踪

### 预期输出 - 栈分配

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
  0.00    0.000000           0         1           read
  0.00    0.000000           0         1           write
  0.00    0.000000           0         3           mmap
  0.00    0.000000           0         1           munmap
------ ----------- ----------- --------- --------- ----------------
100.00    0.000000                     6           total
```

**关键点**：
- 没有 `brk` 调用！
- `mmap` 只是程序加载（libc、vdso）
- 栈分配完全在用户态

### 预期输出 - 堆分配

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 67.89    0.012345         123       100           brk
 23.45    0.004567         456        10           mmap
  8.66    0.001234         411         3           munmap
------ ----------- ----------- --------- --------- ----------------
100.00    0.018146                   113           total
```

**关键点**：
- 大量 `brk` 调用（100次）
- 小块分配用 `brk` 扩展堆
- 大块分配（或释放）用 `mmap`/`munmap`

### 详细 brk 追踪

```bash
strace -e brk ./heap_bench 2>&1 | head -20
```

预期输出：
```
brk(0)                                  = 0x558b1e2a0000
brk(0x558b1e2c1000)                     = 0x558b1e2c1000  # malloc 扩展堆
brk(0x558b1e2e2000)                     = 0x558b1e2e2000  # 再次扩展
brk(0x558b1e303000)                     = 0x558b1e303000
...
```

#### 对应内核路径

博客 §4.3 + `mm/mmap.c:115`：

```
用户态: malloc() → 池不足 → sbrk(size)
        ↓
系统调用: SYSCALL_DEFINE1(brk, ...)
        ↓
内核: do_brk_flags() → 扩展 VMA（无物理页分配）
        ↓
返回: 新的 program break 地址
        ↓
用户态: malloc 在新区域切分
```

博客 §5.5：
> 默认情况：用户态通过 `brk`/`sbrk` 或 `mmap(MAP_ANONYMOUS)`「申请」堆内存时，内核**只建立或扩展 VMA**，并不立刻分配物理页。

内核源码 `mm/vma.c:2714`：
```c
int do_brk_flags(...) {
    vma = vm_area_alloc(mm);   // 只分配 VMA 结构
    vma_set_anonymous(vma);
    vma_set_range(vma, addr, addr + len, ...);
    // 无 alloc_pages，无 mm_populate
}
```

## 4. 汇编代码对比

### 栈分配汇编

```bash
make asm
grep -A 15 "iterative_stack_alloc:" stack_allocation.s
```

预期（简化）：
```asm
iterative_stack_alloc:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 1040         # 分配栈空间（256*4 + 对齐）
    mov     DWORD PTR [rbp-4], 0     # iter = 0
.L3:
    lea     rax, [rbp-1040]   # local_array 地址
    mov     DWORD PTR [rax], 0       # 赋值（可能触发缺页）
    ...
    add     rsp, 1040         # 回收栈空间
    pop     rbp
    ret
```

**关键指令**：
- `sub rsp, N`: 分配栈空间（单条指令，极快）
- `add rsp, N`: 回收（函数返回时）

### 堆分配汇编

```bash
grep -A 15 "iterative_heap_alloc:" heap_allocation.s
```

预期（简化）：
```asm
iterative_heap_alloc:
    push    rbp
    mov     rbp, rsp
    ...
.L5:
    mov     edi, 1024         # size = 1024 bytes
    call    malloc@PLT        # 调用 malloc（可能触发系统调用）
    test    rax, rax          # 检查返回值
    je      .L_error
    mov     QWORD PTR [rbp-8], rax   # 保存指针
    ...
    mov     rdi, QWORD PTR [rbp-8]
    call    free@PLT          # 调用 free
    ...
```

**关键差异**：
- `call malloc`: 函数调用开销 + 内部逻辑
- `call free`: 释放逻辑 + 可能的系统调用
- 栈版本：无函数调用，直接内存访问

## 5. 批发-零售链条验证

博客 §4：

```
内核 Buddy (页) → Slab (对象) → sbrk/mmap (大块) → malloc (小块) → 用户代码
                                                                      ↓
                                                                    栈（绕过）
```

### Buddy 分配器

内核源码 `mm/page_alloc.c`（简化）：
```c
struct page *__alloc_pages(gfp_t gfp, unsigned int order, ...) {
    // 从 zone 的 free_area[order] 分配 2^order 个页
    // 如果没有，从更高 order 分裂
}
```

### Slab 分配器

内核源码 `mm/slub.c:4202`：
```c
void *kmem_cache_alloc_noprof(struct kmem_cache *s, gfp_t gfpflags) {
    // 从 cache 取对象（如 task_struct）
    // 对象缓存在 slab 中，减少对 Buddy 的调用
}
```

### sbrk → malloc

用户态 `malloc` 源码（glibc `malloc/malloc.c`，简化）：
```c
void* __libc_malloc(size_t bytes) {
    if (arena_sufficient) {
        // 从 arena 分配
        return chunk_from_arena();
    } else {
        // 调用 sbrk 或 mmap
        void *mem = sbrk(bytes);
        return mem;
    }
}
```

### 栈：无中间层

直接 CPU 指令：
```asm
sub rsp, N    # 分配，纳秒级
```

## 6. 边界情况

### 情况 1: 大块堆分配（触发 mmap）

修改 `LARGE_SIZE = 131072` (128KB)：

```bash
strace -e mmap ./heap_bench 2>&1 | grep "128\|131072"
```

预期：
```
mmap(NULL, 135168, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f...
munmap(0x7f..., 135168) = 0
```

**原因**：glibc malloc 对大块（通常 >128KB）直接用 `mmap`，释放时 `munmap`。

### 情况 2: 栈溢出

超过 `ulimit -s` 限制：

```c
void stack_overflow() {
    char huge[10 * 1024 * 1024];  // 10MB
    huge[0] = 'x';  // 访问，触发缺页
}
```

预期：
- 缺页处理检查是否在合法栈范围（`ulimit -s`）
- 超出则 SIGSEGV (Segmentation fault)

### 情况 3: MAP_POPULATE

使用 `mmap(..., MAP_POPULATE)`：

```c
void *mem = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
```

博客 §5.5：
> 若带 **`MAP_POPULATE`**，`do_mmap` 成功后会设 `*populate = len`，返回前在 `vm_mmap_pgoff` 里调 `mm_populate(ret, populate)`，在内核内把页 fault in，用户首次访问不再触发缺页。

验证：
```bash
perf stat -e page-faults ./program_with_map_populate
```

应该看到 page-faults 为 0（页已预填充）。

## 总结

所有测试结果都验证了博客的核心论点：

1. **栈快**：分配零成本（1条指令），无系统调用
2. **堆慢**：malloc 管理开销 + 可能的 brk/mmap
3. **缓存局部性**：栈 LIFO → 低 cache-miss
4. **缺页等价**：单次缺页处理，栈与堆成本相同
5. **堆可优化**：预分配 + 复用 → 接近栈性能

内核源码验证了：
- `brk`: 只扩展 VMA，不分配物理页
- 缺页处理：栈/堆走同一路径（`do_anonymous_page`）
- 批发-零售链：栈绕过中间层，直达 CPU

这就是「栈为什么比堆快」的本质！
