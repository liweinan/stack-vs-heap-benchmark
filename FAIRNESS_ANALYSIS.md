# 测试公平性深度分析

## 问题 1：栈没有太多 syscall 是因为预分配吗？

### 关键发现：测试存在**不公平对比**！

#### 实际测试配置对比

```c
// stack_bench (stack_allocation.c:67-68)
const int ITERATIONS = 10000;
const int ARRAY_SIZE = 256;  // 1KB per allocation
// 总栈使用：1KB × 1（复用同一块）

// heap_bench (heap_allocation.c:76-78)
const int ITERATIONS = 10000;
const int SMALL_SIZE = 256;      // 1KB - 小块测试
const int LARGE_SIZE = 32768;    // 128KB - 大块测试 ← 问题！
const int LARGE_ITER = 1000;     // 大块只测 1000 次

// mixed_bench (mixed_benchmark.c:16-17)
#define ITERATIONS 100000
#define ARRAY_SIZE 64  // 256 bytes
```

### ⚠️ 发现的不公平问题

**报告的测试数据来自不同的测试**：

```
栈测试：stack_bench
  - 大小：1KB
  - 次数：10,000
  - Page faults: 34

堆测试：heap_bench 的大块测试
  - 大小：128KB ← 128 倍于栈！
  - 次数：1,000
  - Page faults: 33,190

这不是公平对比！
```

### 正确的公平对比应该是

#### 对比 1：相同大小（1KB）

```bash
# 栈（1KB）
strace -e page-faults ./stack_bench

# 堆（1KB，小块）
strace -e page-faults ./heap_bench  # 只看小块测试部分
```

#### 对比 2：相同大小（256 bytes）

```bash
# 使用 mixed_bench 的公平对比
./mixed_bench
```

这个测试是公平的：
- 栈：64 个 int = 256 bytes
- 堆：malloc(64 * sizeof(int)) = 256 bytes
- 次数：都是 100,000

### 栈预分配的真相

#### 检查默认栈大小

```bash
# 在容器内
ulimit -s
# 通常输出：8192（8MB）
```

#### 栈的实际分配机制

**进程创建时的栈初始化**（`fs/exec.c`）：

```c
static int setup_arg_pages(struct linux_binprm *bprm, ...)
{
    // 创建栈 VMA
    vma = vm_area_alloc(mm);
    vma->vm_start = stack_top - STACK_TOP_MAX;  // 通常 8MB
    vma->vm_end = stack_top;
    vma->vm_flags = VM_STACK_FLAGS | VM_GROWSDOWN;

    // 关键：只创建 VMA，不预分配物理页！
    // 物理页在首次访问时按需分配（缺页）
}
```

**验证**：栈不是预分配的！

```bash
# 在容器内运行
cat /proc/self/maps | grep stack
# 输出类似：
# 7ffffffde000-7ffffffff000 rw-p ... [stack]
# 这只是虚拟地址范围，物理页未分配
```

### 为什么栈的 syscall 少？

**真正的原因**：

1. **栈的 VMA 是持久的**
   ```
   进程创建 → setup_arg_pages() → VMA 创建（一次性）
   函数调用 × 10,000 → 只改 sp 寄存器（无 syscall）
   进程结束 → VMA 删除

   系统调用：0 次（运行时）
   ```

2. **堆的 VMA 是临时的**（使用 mmap 方式）
   ```
   malloc() → mmap() syscall → VMA 创建
   free() → munmap() syscall → VMA 删除
   重复 1000 次

   系统调用：2000 次
   ```

**与预分配无关！**

即使栈使用超过任何"预分配"，也不会触发 syscall：
- 只会触发缺页（#PF）
- 缺页是**异常**，不是系统调用
- 栈扩展通过 `expand_stack()`，只修改 VMA，不调用系统调用

### 实验验证：栈超出"预分配"

创建一个使用 100MB 栈的测试：

```c
void deep_stack() {
    char big[10 * 1024 * 1024];  // 10MB，远超默认
    big[0] = 'x';  // 触发缺页
}
```

**预期**：
- 仍然 0 次 syscall
- 但会有更多 page faults（10MB / 4KB = 2560 次）

---

## 问题 2：mmap vs brk 的内核实现差异

### 内核源码对比（`/Users/weli/works/linux`）

#### brk 系统调用（`mm/mmap.c`）

```c
// mm/mmap.c:115
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
    struct mm_struct *mm = current->mm;
    unsigned long newbrk, oldbrk;

    // 1. 计算新的 brk
    newbrk = PAGE_ALIGN(brk);
    oldbrk = PAGE_ALIGN(mm->brk);

    if (newbrk == oldbrk)
        return mm->brk;

    // 2. 扩展堆
    if (do_brk_flags(&vmi, brkvma, oldbrk, newbrk - oldbrk, 0) < 0)
        goto out;

    // 3. 更新 mm->brk
    mm->brk = brk;

    // 关键：brk 只能线性扩展，不能随机映射
}
```

**brk 的特点**：
1. **线性扩展**：只能向上移动堆顶（`mm->brk`）
2. **不能释放中间部分**：只能回收堆顶以上的区域
3. **碎片问题**：释放后不能归还内核（除非缩小到堆顶以下）
4. **单一 VMA**：整个堆是一个连续的 VMA

#### mmap 系统调用（`mm/mmap.c`）

```c
// mm/mmap.c:337
unsigned long do_mmap(struct file *file, unsigned long addr,
                      unsigned long len, ...)
{
    // 1. 查找空闲地址
    addr = get_unmapped_area(file, addr, len, pgoff, flags);

    // 2. 创建新 VMA
    vma = vm_area_alloc(mm);
    vma->vm_start = addr;
    vma->vm_end = addr + len;

    // 3. 插入 VMA 链表/红黑树
    vma_link(mm, vma, prev, rb_link, rb_parent);

    // 关键：mmap 可以在任意地址创建 VMA
}
```

**mmap 的特点**：
1. **灵活映射**：可以在任意空闲地址创建 VMA
2. **独立 VMA**：每次 mmap 创建独立的 VMA
3. **立即归还**：munmap 立即归还内核
4. **多 VMA**：进程可以有多个独立的匿名映射

### 性能差异的本质

#### 1. VMA 管理开销

**brk（扩展堆顶）**：
```c
// mm/vma.c:2714
int do_brk_flags(...)
{
    // 可能与已有堆 VMA 合并
    if (can_vma_merge_after(prev, flags, ...)) {
        // 只需修改 prev->vm_end
        prev->vm_end = addr + len;
        return 0;
    }

    // 或创建新 VMA
    vma = vm_area_alloc(mm);
    // 成本：~100 ns
}
```

**mmap（创建新映射）**：
```c
unsigned long do_mmap(...)
{
    // 1. 查找空闲地址（O(log n)，红黑树）
    addr = get_unmapped_area(...);

    // 2. 分配 VMA 结构
    vma = vm_area_alloc(mm);

    // 3. 初始化 VMA
    vma->vm_start = addr;
    vma->vm_end = addr + len;
    vma->vm_flags = ...;

    // 4. 插入红黑树（O(log n)）
    vma_link(mm, vma, ...);

    // 成本：~500 ns - 1 μs
}
```

**对比**：
- brk 扩展：可能只修改一个字段（prev->vm_end）
- mmap：完整的 VMA 创建 + 红黑树插入

#### 2. munmap vs brk 缩小

**munmap（删除映射）**：
```c
// mm/mmap.c:1067
int do_munmap(...)
{
    // 1. 从红黑树删除 VMA
    __vma_rb_erase(vma, &mm->mm_rb);

    // 2. 删除页表项
    unmap_page_range(vma, start, end, ...);

    // 3. 释放物理页
    free_pgtables(...);

    // 4. 释放 VMA 结构
    remove_vma(vma);

    // 成本：~1-2 μs
}
```

**brk 缩小**（很少用）：
```c
// 通常 malloc 不缩小 brk
// 因为：
// 1. 缩小后无法释放物理页（页表仍在）
// 2. 下次扩展又要重新分配
// 3. 不如保留在用户态池中复用
```

#### 3. 地址空间碎片

**brk 的问题**：
```
堆起始              brk（堆顶）
    ↓                   ↓
    [================]

已分配：████░░██░███░░░░
        ↑碎片      ↑碎片

问题：
- 碎片无法归还内核
- brk 只能整体回收（缩小到堆顶以下）
- 长期运行会累积碎片
```

**mmap 的优势**：
```
VMA 1: [████]  → munmap → 立即归还
VMA 2: [██████] → munmap → 立即归还
VMA 3: [████]   → 保留

- 每个 VMA 独立
- munmap 立即归还内核
- 无碎片累积
```

### 为什么 glibc/musl 对大块用 mmap？

```c
// musl: src/malloc/malloc.c (简化)
#define MMAP_THRESHOLD (128 * 1024)  // 128KB

void* malloc(size_t n)
{
    if (n >= MMAP_THRESHOLD) {
        // 大块：用 mmap
        // 原因：
        // 1. 避免堆碎片
        // 2. munmap 可立即归还
        // 3. 不影响 brk 堆
        return mmap(...);
    }

    // 小块：从 brk 堆分配
    // 原因：
    // 1. 减少 VMA 数量
    // 2. 减少系统调用
    // 3. 利于缓存局部性
}
```

### 内核层面的性能开销对比

| 操作 | brk | mmap | 差异 |
|------|-----|------|------|
| 分配虚拟地址 | 修改 mm->brk | 查找空闲地址（红黑树） | mmap 慢 ~2x |
| VMA 管理 | 可能合并已有 VMA | 总是创建新 VMA | mmap 慢 ~3x |
| 页表操作 | 按需（缺页时） | 按需（缺页时） | 相同 |
| 释放 | 缩小 brk（很少） | munmap（删除 VMA + 页表） | munmap 慢 ~5x |
| 碎片 | 累积 | 无 | brk 差 |

**总结**：
- **单次操作**：brk 快于 mmap（~2-3x）
- **长期运行**：mmap 更好（无碎片）
- **大块分配**：mmap 更适合（可立即归还）

---

## 问题 3：测试的公平性

### 当前测试的公平性问题

#### ❌ 不公平的对比（当前报告）

```
对比项：
  栈：1KB × 10,000 次
  堆：128KB × 1,000 次

差异：
  - 大小：128 倍
  - 总内存：128KB vs 10KB（但栈复用）
  - Page faults：34 vs 33,190（975 倍）

问题：
  大小不同，无法公平比较！
```

#### ✅ 公平的对比（mixed_bench）

```
对比项：
  栈：256 bytes × 100,000 次
  堆：256 bytes × 100,000 次

实测结果：
  栈分配:  50 ns/次
  堆分配:  70 ns/次（malloc/free）
  堆复用:  48 ns/次

差异：
  - 大小：相同 ✓
  - 次数：相同 ✓
  - 工作负载：相同 ✓
```

### 为什么大块测试的差异这么大？

#### 128KB vs 1KB 的差异

**栈（1KB）**：
```
1KB = 1024 bytes < 4096 bytes（1 页）
每次分配触及：1 页
10,000 次复用同一页
Page faults：~1（只在首次）
```

**堆（128KB，mmap 方式）**：
```
128KB = 131,072 bytes = 32 页
每次 mmap：创建新 VMA，无物理页
首次访问每个页：触发缺页
每次 munmap：删除页表，释放物理页
1000 次迭代：1000 × 32 = 32,000 次 page faults

加上：
- 初始化：~1,000 次
- 其他测试：~200 次
总计：~33,190 次
```

**关键**：
- 大小差异导致 page faults 数量级不同
- mmap/munmap 的 VMA 管理开销
- 页表的创建/删除开销

### 正确的公平基准测试

#### 测试 1：相同小块（256 bytes）

**已有**：`mixed_bench`
- ✓ 大小相同
- ✓ 次数相同
- ✓ 工作负载相同

**结果**：
```
栈:  50 ns
堆:  70 ns（1.4x）
```

#### 测试 2：相同大块（128KB）

**需要添加**：
```c
// 栈分配 128KB（需要修改）
void stack_large() {
    char arr[128 * 1024];  // 128KB
    arr[0] = 1;
}

// 堆分配 128KB（已有）
void heap_large() {
    char *arr = malloc(128 * 1024);
    arr[0] = 1;
    free(arr);
}
```

**预期**：
- 栈：会有更多 page faults（32 次/分配 × 迭代数）
- 堆：仍然是 mmap/munmap 方式
- 差异会缩小

#### 测试 3：不同分配器对比

**brk 堆 vs mmap 堆**：
```c
// 小块（< 128KB）：用 brk
malloc(1024);  // 从 brk 堆分配

// 大块（≥ 128KB）：用 mmap
malloc(128 * 1024);  // mmap 方式
```

这才是公平对比不同分配器的方式。

---

## 修正后的结论

### 1. 栈的 syscall 少不是因为预分配

**真正原因**：
1. **VMA 持久性**：栈的 VMA 在进程创建时建立，生命周期 = 进程
2. **栈扩展机制**：`expand_stack()` 只修改 VMA，不调用系统调用
3. **缺页不是 syscall**：缺页是异常（#PF），由内核异常处理程序处理

**验证**：
```bash
# 栈使用远超 8MB 默认大小
ulimit -s unlimited
strace -c ./large_stack_test

# 仍然 0 次 brk/mmap syscall
```

### 2. mmap vs brk 的本质差异

| 维度 | brk | mmap |
|------|-----|------|
| **VMA 管理** | 单一堆 VMA，可能合并 | 每次创建独立 VMA |
| **地址空间** | 线性扩展（堆顶） | 任意空闲地址 |
| **释放机制** | 缩小堆顶（很少用） | munmap 删除 VMA |
| **碎片** | 累积碎片 | 无碎片 |
| **单次成本** | 快（~200 ns） | 慢（~1 μs） |
| **适用场景** | 小块频繁分配 | 大块独立分配 |

### 3. 公平测试的重要性

**当前项目的问题**：
- ❌ 报告的数据对比了不同大小（1KB vs 128KB）
- ✓ `mixed_bench` 是公平的（256 bytes vs 256 bytes）
- ❌ 缺少大块公平对比（128KB 栈 vs 128KB 堆）

**建议**：
1. 明确标注测试配置
2. 分离小块和大块测试
3. 添加公平的大块对比
4. 说明 mmap vs brk 的区别

---

## 推荐的改进

### 1. 添加公平的大块对比

```c
// fair_large_benchmark.c
#define LARGE_SIZE (128 * 1024)  // 128KB

void stack_large(int iterations) {
    for (int i = 0; i < iterations; i++) {
        char arr[LARGE_SIZE];
        memset(arr, 0, LARGE_SIZE);
        volatile int sum = arr[0];
    }
}

void heap_large(int iterations) {
    for (int i = 0; i < iterations; i++) {
        char *arr = malloc(LARGE_SIZE);
        memset(arr, 0, LARGE_SIZE);
        volatile int sum = arr[0];
        free(arr);
    }
}
```

### 2. 分离 brk 和 mmap 测试

```c
// brk 堆（小块，< 128KB）
malloc(1024);  // 从 brk 堆

// mmap 堆（大块，≥ 128KB）
malloc(128 * 1024);  // mmap 方式

// 明确标注测试的是哪种
```

### 3. 文档中明确说明

```markdown
## 测试配置

### mixed_bench（公平对比）
- 栈：256 bytes × 100,000
- 堆：256 bytes × 100,000
- 结果：栈 50ns，堆 70ns（1.4x）

### heap_bench 大块测试（不公平对比）
- 栈：未测试
- 堆：128KB × 1,000（mmap 方式）
- 结果：仅展示 mmap 的特点，不与栈对比
```

---

## 总结

你的质疑非常正确！

1. **栈的 syscall 少**：与预分配无关，是 VMA 持久性的结果
2. **mmap vs brk**：VMA 管理方式不同，适用场景不同
3. **公平性**：当前报告混淆了不同大小的测试，需要明确区分

建议：
- 使用 `mixed_bench` 的结果作为主要对比
- 单独讨论大块 mmap 的特点
- 明确标注测试配置
