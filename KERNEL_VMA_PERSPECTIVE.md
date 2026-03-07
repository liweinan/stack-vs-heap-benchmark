# 内核视角：Stack vs Heap 的本质区别

## 核心观点

**Kernel 不区分 "stack" vs "heap"，只区分 VMA 的类型和生命周期！**

```
用户态概念：         Stack              Heap
                     ↓                  ↓
内核实际看到：    VMA (栈类型)      VMA (匿名映射)
                VM_GROWSDOWN      MAP_ANONYMOUS
                生命周期=进程      生命周期=malloc/free
```

---

## 内核眼中的"栈"：特殊标记的 VMA

### 1. 栈 VMA 的创建（进程启动时）

**源码**：`fs/exec.c:778`

```c
static int setup_arg_pages(struct linux_binprm *bprm,
                           unsigned long stack_top,
                           int executable_stack)
{
    unsigned long stack_shift;
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;

    // 分配 VMA 结构
    vma = vm_area_alloc(mm);
    if (!vma)
        return -ENOMEM;

    // 设置栈的虚拟地址范围
    vma->vm_start = stack_top - STACK_TOP_MAX;  // 通常 8MB
    vma->vm_end = stack_top;

    // 关键：设置 VM_GROWSDOWN 标志
    vma->vm_flags = VM_STACK_FLAGS | VM_GROWSDOWN;
    //              ^^^^^^^^^^^^^^
    //              这是栈与普通 VMA 的唯一区别！

    vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

    // 插入到进程的 VMA 链表
    insert_vm_struct(mm, vma);

    mm->stack_vm += vma_pages(vma);

    return 0;
}
```

**关键点**：
1. **VM_GROWSDOWN**：告诉内核这是一个"向下增长"的 VMA
2. **生命周期**：VMA 在进程创建时建立，进程退出时销毁
3. **物理页**：此时 **没有分配任何物理页**

### 2. 栈 VMA 的标志定义

**源码**：`include/linux/mm.h:286`

```c
#define VM_GROWSDOWN    0x00000100  /* 向下增长（栈） */
#define VM_GROWSUP      0x00000200  /* 向上增长（某些架构的栈） */

#define VM_STACK_FLAGS (VM_GROWSDOWN | VM_ACCOUNT)
```

**对比其他 VMA 类型**：

```c
// 普通匿名映射（mmap 分配的堆）
flags = MAP_ANONYMOUS | MAP_PRIVATE
→ 内核标志: 0（无特殊标志）

// 文件映射
flags = MAP_SHARED
→ 内核标志: VM_SHARED

// 栈
→ 内核标志: VM_GROWSDOWN
```

---

## 内核眼中的"堆"：普通匿名 VMA

### 1. brk 堆的创建

**源码**：`mm/mmap.c:115`

```c
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
    struct mm_struct *mm = current->mm;
    unsigned long newbrk, oldbrk;

    // 计算新旧 brk
    newbrk = PAGE_ALIGN(brk);
    oldbrk = PAGE_ALIGN(mm->brk);

    // 扩展堆（可能创建新 VMA 或扩展已有 VMA）
    if (do_brk_flags(&vmi, brkvma, oldbrk, newbrk - oldbrk, 0) < 0)
        goto out;

    // 更新 mm->brk
    mm->brk = brk;

    return brk;
}
```

**关键点**：
1. **无特殊标志**：brk VMA 没有 VM_GROWSDOWN 等标志
2. **生命周期**：VMA 在首次 brk() 时创建，进程退出时销毁
3. **区别**：brk VMA 是持久的（像栈），但没有特殊标志

### 2. mmap 堆的创建

**源码**：`mm/mmap.c:337`

```c
unsigned long do_mmap(struct file *file, unsigned long addr,
                      unsigned long len, unsigned long prot,
                      unsigned long flags, unsigned long pgoff)
{
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;

    // 查找空闲地址
    addr = get_unmapped_area(file, addr, len, pgoff, flags);

    // 分配 VMA
    vma = vm_area_alloc(mm);
    vma->vm_start = addr;
    vma->vm_end = addr + len;
    vma->vm_flags = calc_vm_flag_bits(flags);  // 根据 mmap flags
    vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

    // 插入 VMA 链表/红黑树
    vma_link(mm, vma, prev, rb_link, rb_parent);

    return addr;
}
```

**关键点**：
1. **无特殊标志**：匿名 mmap 也没有 VM_GROWSDOWN
2. **生命周期**：每次 mmap 创建，munmap 销毁（临时性）
3. **区别**：mmap VMA 是临时的（不像栈或 brk 堆）

---

## 本质区别：VMA 生命周期

### 对比表

| VMA 类型 | 创建时机 | 销毁时机 | 特殊标志 | 生命周期 |
|---------|---------|---------|---------|---------|
| **栈 VMA** | 进程启动 | 进程退出 | VM_GROWSDOWN | 进程级别 |
| **brk 堆 VMA** | 首次 brk() | 进程退出 | 无 | 进程级别 |
| **mmap 堆 VMA** | 每次 mmap() | 每次 munmap() | 无 | malloc/free 级别 |
| **文件映射 VMA** | mmap(文件) | munmap() | VM_SHARED 等 | 手动管理 |

### 可视化

```
进程生命周期：
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  栈 VMA (VM_GROWSDOWN)                                      │
│  ┌───────────────────────────────────────────────────────┐ │
│  │ [stack] 0x7fff_0000 - 0x7fff_ffff                     │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  brk 堆 VMA                                                 │
│  ┌───────────────────────────────────────────────────────┐ │
│  │ [heap] 0x0055_5000 - 0x0057_6000 (可扩展)             │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  mmap 堆 VMA (临时)                                         │
│  ┌─────┐    ┌─────┐    ┌─────┐                            │
│  │ VMA1│    │ VMA2│    │ VMA3│  ← 每次 malloc 创建         │
│  └─────┘    └─────┘    └─────┘    每次 free 销毁          │
│    ↑          ↑          ↑                                 │
│  创建       销毁       创建                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Kernel 如何处理不同 VMA

### 1. 缺页处理（所有 VMA 相同）

**源码**：`mm/memory.c:5022`

```c
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct folio *folio;

    // 分配物理页（栈、brk堆、mmap堆都一样！）
    folio = alloc_anon_folio(vmf);
    if (!folio)
        goto oom;

    // 清零（栈、brk堆、mmap堆都一样！）
    __folio_mark_uptodate(folio);

    // 建立页表映射（栈、brk堆、mmap堆都一样！）
    entry = folio_mk_pte(folio, vma->vm_page_prot);
    set_ptes(vma->vm_mm, addr, vmf->pte, entry, nr_pages);

    // 关键：kernel 不关心这是栈还是堆！
    // 只要是匿名页，处理流程完全相同

    return 0;
}
```

**结论**：**缺页处理路径，栈与堆完全相同！**

### 2. 栈扩展（仅栈 VMA 特殊）

**源码**：`mm/mmap.c:961`

```c
int expand_stack_locked(struct vm_area_struct *vma, unsigned long address)
{
    struct mm_struct *mm = vma->vm_mm;

    // 检查是否是栈 VMA
    if (!(vma->vm_flags & VM_GROWSDOWN))
        return -EFAULT;  // 不是栈，拒绝扩展

    // 扩展 VMA 范围
    vma->vm_start = address;

    // 关键：不分配物理页，只修改 VMA！
    // 物理页在后续缺页时分配

    mm->stack_vm += grow;

    return 0;
}
```

**对比 brk 扩展**（`mm/vma.c:2714`）：

```c
int do_brk_flags(struct vma_iterator *vmi, struct vm_area_struct *vma,
                 unsigned long addr, unsigned long len, ...)
{
    // 可能与已有堆 VMA 合并
    if (can_vma_merge_after(prev, flags, ...)) {
        prev->vm_end = addr + len;  // 只修改 vm_end
        return 0;
    }

    // 或创建新 VMA
    vma = vm_area_alloc(mm);
    // ...

    // 关键：也不分配物理页，只修改/创建 VMA
}
```

**结论**：**栈扩展和 brk 扩展都只修改 VMA，不分配物理页！**

---

## 真正的性能差异来源

### 不是 "kernel 对 stack 和 heap 处理不同"

而是：

### 1. VMA 生命周期不同

```c
// 栈（进程级别）
setup_arg_pages()  ← 进程启动时创建 VMA
// ... 10,000 次函数调用，VMA 不变
exit_mmap()        ← 进程退出时销毁 VMA

系统调用：0 次（运行时）
```

vs

```c
// mmap 堆（malloc 级别）
for (i = 0; i < 1000; i++) {
    mmap()    ← 创建 VMA
    // ... 使用
    munmap()  ← 销毁 VMA
}

系统调用：2000 次
```

### 2. 页表持久性不同

```c
// 栈
expand_stack_locked() {
    vma->vm_start = address;  // 只修改 VMA
    // 页表项保留！
}
```

vs

```c
// mmap 堆
do_munmap() {
    unmap_page_range();   // 删除页表项
    free_pgtables();      // 释放页表
    remove_vma();         // 删除 VMA
    // 页表完全清除！
}
```

### 3. 分配机制不同

```c
// 栈（CPU 指令）
sub sp, sp, #1024  ← 1 条指令，~0.3 ns

// 堆（系统调用）
malloc() → mmap() → do_mmap() → ... ← ~1 μs
```

---

## 验证：查看进程的 VMA

### 示例

```bash
# 运行程序
$ ./stack_bench &
[1] 12345

# 查看进程的内存映射
$ cat /proc/12345/maps
00400000-00401000 r-xp 00000000 08:01 123456  /workspace/stack_bench
00600000-00601000 r--p 00000000 08:01 123456  /workspace/stack_bench
00601000-00602000 rw-p 00001000 08:01 123456  /workspace/stack_bench
00602000-00623000 rw-p 00000000 00:00 0       [heap]        ← brk 堆 VMA
7f1234567000-7f1234588000 r-xp 00000000 08:01 234567  /lib/libc.so
7f1234788000-7f123478c000 rw-p 00021000 08:01 234567  /lib/libc.so
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0       [stack]       ← 栈 VMA
```

**关键观察**：
1. `[heap]` 和 `[stack]` 是内核标记
2. 但对内核来说，都是 VMA
3. 唯一区别：`vm_flags` 字段（VM_GROWSDOWN vs 普通）

### 验证特殊标志

```bash
# 查看 VMA 详细信息
$ cat /proc/12345/smaps | grep -A 20 "\[stack\]"
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0       [stack]
Size:               132 kB
Rss:                 12 kB
Pss:                 12 kB
Shared_Clean:         0 kB
Shared_Dirty:         0 kB
Private_Clean:        0 kB
Private_Dirty:       12 kB
Referenced:          12 kB
Anonymous:           12 kB    ← 匿名页（与 mmap 堆相同）
AnonHugePages:        0 kB
Swap:                 0 kB
VmFlags: rd wr mr mw me gd ac   ← gd = growsdown（这是唯一区别！）
```

---

## 总结

### Kernel 的视角

```
用户态：                "这是栈"         "这是堆"
                          ↓                ↓
Kernel：           VMA + VM_GROWSDOWN    VMA（普通）
                          ↓                ↓
                      相同的处理：
                    - 缺页 → do_anonymous_page()
                    - 物理页分配 → alloc_anon_folio()
                    - 页表映射 → set_ptes()
```

### 真正的区别

| 维度 | 栈 VMA | brk 堆 VMA | mmap 堆 VMA |
|------|--------|-----------|------------|
| **VMA 标志** | VM_GROWSDOWN | 无 | 无 |
| **生命周期** | 进程级别 | 进程级别 | malloc/free 级别 |
| **创建/销毁** | 进程启动/退出 | 首次 brk/进程退出 | 每次 mmap/munmap |
| **扩展方式** | expand_stack | do_brk | 无（固定大小） |
| **页表持久性** | 持久（扩展时保留） | 持久（扩展时保留） | 临时（munmap 删除） |
| **缺页处理** | do_anonymous_page | do_anonymous_page | do_anonymous_page |

### 性能差异的本质

**不是 kernel 对 stack 和 heap 的处理不同！**

**而是 VMA 的生命周期管理方式不同：**
1. 栈/brk 堆：VMA 持久，函数调用/malloc 只触发缺页
2. mmap 堆：VMA 临时，每次 malloc/free 都要 mmap/munmap

**这导致**：
- mmap 堆：大量系统调用（VMA 创建/销毁）
- 栈：0 次系统调用（VMA 已存在）
- 缺页次数差异：页表持久性不同

---

## 参考内核源码

- `fs/exec.c:778` - setup_arg_pages()（创建栈 VMA）
- `mm/mmap.c:115` - sys_brk()（brk 堆管理）
- `mm/mmap.c:337` - do_mmap()（mmap 堆创建）
- `mm/mmap.c:961` - expand_stack_locked()（栈扩展）
- `mm/memory.c:5022` - do_anonymous_page()（缺页处理）
- `include/linux/mm.h:286` - VM_GROWSDOWN 定义
