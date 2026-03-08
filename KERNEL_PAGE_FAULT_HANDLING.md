# Linux Kernel 缺页处理机制（基于最新代码）

> **代码来源**: Linux kernel 主线代码 (arch/x86/mm/fault.c, mm/memory.c)
> **架构**: x86_64

## 🎯 快速参考：调用链总览

```c
// ✅ 准确的调用链（带源码位置）
exc_page_fault()                    // arch/x86/mm/fault.c:1488
  → handle_page_fault()             // arch/x86/mm/fault.c:1464
    → do_user_addr_fault()          // arch/x86/mm/fault.c:1209
      → handle_mm_fault()           // mm/memory.c:6346
        → __handle_mm_fault()       // mm/memory.c:6119
          → handle_pte_fault()      // mm/memory.c:6025
            → do_pte_missing()      // mm/memory.c:4246
              → do_anonymous_page() // mm/memory.c:5022
                {
                    folio = alloc_anon_folio(vmf);      // 分配物理页
                    entry = folio_mk_pte(folio, ...);   // 创建 PTE
                    set_ptes(..., entry, nr_pages);     // 设置页表
                    update_mmu_cache_range(...);        // 刷新 TLB
                }
```

**关键步骤**：
1. **异常处理** (1488→1464→1209): CPU #PF → 读 CR2 → 查找 VMA
2. **页表遍历** (6346→6119→6025): PGD → P4D → PUD → PMD → PTE
3. **缺页分发** (4246): 区分匿名页 vs 文件页
4. **物理分配** (5022): 分配 folio → 建立映射 → 刷新 TLB

---

## 完整调用链

### 1. 异常入口：exc_page_fault()
**文件**: `arch/x86/mm/fault.c:1488`

```c
DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)
{
    irqentry_state_t state;
    unsigned long address;

    // 读取出错地址（CR2 寄存器）
    address = cpu_feature_enabled(X86_FEATURE_FRED) ?
              fred_event_data(regs) : read_cr2();

    // 处理 KVM 异步缺页
    if (kvm_handle_async_pf(regs, (u32)address))
        return;

    state = irqentry_enter(regs);
    instrumentation_begin();

    // 调用核心处理函数
    handle_page_fault(regs, error_code, address);

    instrumentation_end();
    irqentry_exit(regs, state);
}
```

**关键点**:
- CPU 触发 #PF (Page Fault) 异常时的入口
- 通过 `CR2` 寄存器获取导致缺页的虚拟地址
- `error_code` 包含缺页原因（读/写、用户/内核、页不存在/权限错误等）

---

### 2. 分发处理：handle_page_fault()
**文件**: `arch/x86/mm/fault.c:1464`

```c
handle_page_fault(struct pt_regs *regs, unsigned long error_code,
                  unsigned long address)
{
    trace_page_fault_entries(regs, error_code, address);

    if (unlikely(kmmio_fault(regs, address)))
        return;

    // 判断是内核地址还是用户地址
    if (unlikely(fault_in_kernel_space(address))) {
        do_kern_addr_fault(regs, error_code, address);
    } else {
        do_user_addr_fault(regs, error_code, address);
        local_irq_disable();
    }
}
```

**关键点**:
- 区分内核空间和用户空间的缺页
- 内核空间 → `do_kern_addr_fault()`
- 用户空间 → `do_user_addr_fault()`

---

### 3. 用户地址处理：do_user_addr_fault()
**文件**: `arch/x86/mm/fault.c:1209`

```c
void do_user_addr_fault(struct pt_regs *regs,
                        unsigned long error_code,
                        unsigned long address)
{
    struct vm_area_struct *vma;
    struct task_struct *tsk;
    struct mm_struct *mm;
    vm_fault_t fault;
    unsigned int flags = FAULT_FLAG_DEFAULT;

    tsk = current;
    mm = tsk->mm;

    // ... 各种安全检查 ...

    // 设置 fault 标志
    if (error_code & X86_PF_WRITE)
        flags |= FAULT_FLAG_WRITE;
    if (error_code & X86_PF_INSTR)
        flags |= FAULT_FLAG_INSTRUCTION;
    if (user_mode(regs))
        flags |= FAULT_FLAG_USER;

retry:
    // 查找出错地址对应的 VMA
    vma = lock_mm_and_find_vma(mm, address, regs);
    if (unlikely(!vma)) {
        bad_area_nosemaphore(regs, error_code, address);
        return;
    }

    // 权限检查
    if (unlikely(access_error(error_code, vma))) {
        bad_area_access_error(regs, error_code, address, mm, vma);
        return;
    }

    // 调用通用 MM 故障处理
    fault = handle_mm_fault(vma, address, flags, regs);

    // 处理返回结果（重试、错误等）
    if (unlikely(fault & VM_FAULT_RETRY)) {
        flags |= FAULT_FLAG_TRIED;
        goto retry;
    }

    mmap_read_unlock(mm);
    // ...
}
```

**关键点**:
- 根据 error_code 设置 FAULT_FLAG_WRITE/READ
- 查找 VMA (Virtual Memory Area) 确定访问是否合法
- 调用通用的 `handle_mm_fault()`

---

### 4. 通用处理：handle_mm_fault()
**文件**: `mm/memory.c:6346`

```c
vm_fault_t handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
                           unsigned int flags, struct pt_regs *regs)
{
    struct mm_struct *mm = vma->vm_mm;
    vm_fault_t ret;

    __set_current_state(TASK_RUNNING);

    // 权限检查
    if (!arch_vma_access_permitted(vma, flags & FAULT_FLAG_WRITE,
                                    flags & FAULT_FLAG_INSTRUCTION,
                                    flags & FAULT_FLAG_REMOTE)) {
        ret = VM_FAULT_SIGSEGV;
        goto out;
    }

    // 内存控制组（cgroup）OOM 处理
    if (flags & FAULT_FLAG_USER)
        mem_cgroup_enter_user_fault();

    lru_gen_enter_fault(vma);

    // 大页处理 or 普通页处理
    if (unlikely(is_vm_hugetlb_page(vma)))
        ret = hugetlb_fault(vma->vm_mm, vma, address, flags);
    else
        ret = __handle_mm_fault(vma, address, flags);

    lru_gen_exit_fault();

    if (flags & FAULT_FLAG_USER) {
        mem_cgroup_exit_user_fault();
        // ...
    }

out:
    return ret;
}
```

**关键点**:
- 进入内存故障处理的通用层
- 处理内存控制组（cgroup）限制
- 区分大页（hugetlb）和普通页

---

### 5. 页表遍历：__handle_mm_fault()
**文件**: `mm/memory.c:6119`

```c
static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
                                    unsigned long address, unsigned int flags)
{
    struct vm_fault vmf = {
        .vma = vma,
        .address = address & PAGE_MASK,
        .real_address = address,
        .flags = flags,
        .pgoff = linear_page_index(vma, address),
        .gfp_mask = __get_fault_gfp_mask(vma),
    };
    struct mm_struct *mm = vma->vm_mm;
    pgd_t *pgd;
    p4d_t *p4d;
    vm_fault_t ret;

    // 遍历页表层次结构
    pgd = pgd_offset(mm, address);              // PGD
    p4d = p4d_alloc(mm, pgd, address);          // P4D (5级页表)
    if (!p4d)
        return VM_FAULT_OOM;

    vmf.pud = pud_alloc(mm, p4d, address);      // PUD
    if (!vmf.pud)
        return VM_FAULT_OOM;

    // ... 处理大页（PUD 级别）...

    vmf.pmd = pmd_alloc(mm, vmf.pud, address);  // PMD
    if (!vmf.pmd)
        return VM_FAULT_OOM;

    // ... 处理大页（PMD 级别，如 2MB THP）...

    // 处理最底层页表（PTE）
    return handle_pte_fault(&vmf);
}
```

**关键点**:
- 遍历 4/5 级页表结构：PGD → P4D → PUD → PMD → PTE
- 按需分配中间层页表（`p4d_alloc`, `pud_alloc`, `pmd_alloc`）
- 如果中间层分配失败返回 `VM_FAULT_OOM`
- 最后调用 `handle_pte_fault()` 处理 PTE 级别

---

### 6. PTE 处理：handle_pte_fault()
**文件**: `mm/memory.c:6025`

```c
static vm_fault_t handle_pte_fault(struct vm_fault *vmf)
{
    pte_t entry;

    if (unlikely(pmd_none(*vmf->pmd))) {
        // PMD 为空，PTE 也不存在
        vmf->pte = NULL;
        vmf->flags &= ~FAULT_FLAG_ORIG_PTE_VALID;
    } else {
        // 获取 PTE
        vmf->pte = pte_offset_map_rw_nolock(vmf->vma->vm_mm, vmf->pmd,
                                            vmf->address, ...);
        if (unlikely(!vmf->pte))
            return 0;

        vmf->orig_pte = ptep_get_lockless(vmf->pte);
        vmf->flags |= FAULT_FLAG_ORIG_PTE_VALID;

        if (pte_none(vmf->orig_pte)) {
            pte_unmap(vmf->pte);
            vmf->pte = NULL;
        }
    }

    // PTE 不存在 → 真正的缺页
    if (!vmf->pte)
        return do_pte_missing(vmf);

    // PTE 存在但页不在内存（被换出）
    if (!pte_present(vmf->orig_pte))
        return do_swap_page(vmf);

    // NUMA 页迁移
    if (pte_protnone(vmf->orig_pte) && vma_is_accessible(vmf->vma))
        return do_numa_page(vmf);

    spin_lock(vmf->ptl);
    entry = vmf->orig_pte;

    // 写时复制（COW）
    if (vmf->flags & (FAULT_FLAG_WRITE|FAULT_FLAG_UNSHARE)) {
        if (!pte_write(entry))
            return do_wp_page(vmf);  // Copy-on-Write
        // ...
    }

    // 更新访问位
    entry = pte_mkyoung(entry);
    // ...
}
```

**关键点**:
- 检查 PTE 是否存在
- `pte_none()` → 页从未分配 → `do_pte_missing()`
- `!pte_present()` → 页被换出到磁盘 → `do_swap_page()`
- `!pte_write() && WRITE` → 写保护页 → `do_wp_page()` (COW)

---

### 7. 缺页分发：do_pte_missing()
**文件**: `mm/memory.c:4246`

```c
static vm_fault_t do_pte_missing(struct vm_fault *vmf)
{
    if (vma_is_anonymous(vmf->vma))
        return do_anonymous_page(vmf);  // 匿名页（栈、堆）
    else
        return do_fault(vmf);           // 文件映射页
}
```

**关键点**:
- 匿名映射（栈、堆、CoW 私有映射）→ `do_anonymous_page()`
- 文件映射（mmap 文件、共享库）→ `do_fault()`

---

### 8. 匿名页分配：do_anonymous_page()
**文件**: `mm/memory.c:5022`

```c
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    unsigned long addr = vmf->address;
    struct folio *folio;
    vm_fault_t ret = 0;
    int nr_pages = 1;
    pte_t entry;

    // 文件共享映射不应该到这里
    if (vma->vm_flags & VM_SHARED)
        return VM_FAULT_SIGBUS;

    // 分配页表（如果还没有）
    if (pte_alloc(vma->vm_mm, vmf->pmd))
        return VM_FAULT_OOM;

    // 只读访问 → 使用零页优化
    if (!(vmf->flags & FAULT_FLAG_WRITE) &&
            !mm_forbids_zeropage(vma->vm_mm)) {
        entry = pte_mkspecial(pfn_pte(my_zero_pfn(vmf->address),
                                      vma->vm_page_prot));
        vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
                                       vmf->address, &vmf->ptl);
        // ... 检查和 userfaultfd 处理 ...
        goto setpte;  // 直接设置 PTE，不分配物理页
    }

    // 分配我们自己的私有页
    ret = vmf_anon_prepare(vmf);
    if (ret)
        return ret;

    // 分配物理页（可能是大页 folio）
    folio = alloc_anon_folio(vmf);
    if (IS_ERR(folio))
        return 0;
    if (!folio)
        goto oom;

    nr_pages = folio_nr_pages(folio);
    addr = ALIGN_DOWN(vmf->address, nr_pages * PAGE_SIZE);

    // 标记页为最新（内存屏障确保顺序）
    __folio_mark_uptodate(folio);

    // 创建 PTE 条目
    entry = folio_mk_pte(folio, vma->vm_page_prot);
    entry = pte_sw_mkyoung(entry);
    if (vma->vm_flags & VM_WRITE)
        entry = pte_mkwrite(pte_mkdirty(entry), vma);

    // 获取 PTE 锁
    vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, addr, &vmf->ptl);
    if (!vmf->pte)
        goto release;

    // 再次检查 PTE 是否还是空的（避免竞争）
    if (nr_pages == 1 && vmf_pte_changed(vmf)) {
        update_mmu_tlb(vma, addr, vmf->pte);
        goto release;
    }
    // ...

    // 增加引用计数
    folio_ref_add(folio, nr_pages - 1);

    // 更新统计
    add_mm_counter(vma->vm_mm, MM_ANONPAGES, nr_pages);
    count_mthp_stat(folio_order(folio), MTHP_STAT_ANON_FAULT_ALLOC);

    // 添加到反向映射（rmap）
    folio_add_new_anon_rmap(folio, vma, addr, RMAP_EXCLUSIVE);

    // 添加到 LRU 链表
    folio_add_lru_vma(folio, vma);

setpte:
    if (vmf_orig_pte_uffd_wp(vmf))
        entry = pte_mkuffd_wp(entry);

    // 设置 PTE（关键！）
    set_ptes(vma->vm_mm, addr, vmf->pte, entry, nr_pages);

    // 更新 MMU 缓存（TLB）
    update_mmu_cache_range(vmf, vma, addr, vmf->pte, nr_pages);

unlock:
    if (vmf->pte)
        pte_unmap_unlock(vmf->pte, vmf->ptl);
    return ret;

release:
    folio_put(folio);
    goto unlock;
oom:
    return VM_FAULT_OOM;
}
```

**关键点**:
1. **零页优化**: 只读访问直接映射到共享的零页（`my_zero_pfn`），不分配物理内存
2. **分配物理页**: `alloc_anon_folio()` - 可能分配大页（THP）
3. **创建 PTE**: `folio_mk_pte()` - 根据 VMA 权限创建页表项
4. **设置页表**: `set_ptes()` - 将 PTE 写入页表（可批量设置多个）
5. **反向映射**: `folio_add_new_anon_rmap()` - 建立物理页→虚拟页的映射
6. **加入 LRU**: `folio_add_lru_vma()` - 用于页面回收

---

### 9. 物理页分配：alloc_anon_folio()
**文件**: `mm/memory.c:4932`

```c
static struct folio *alloc_anon_folio(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
    unsigned long orders;
    struct folio *folio;
    unsigned long addr;
    pte_t *pte;
    gfp_t gfp;
    int order;

    // userfaultfd 需要单页精度
    if (unlikely(userfaultfd_armed(vma)))
        goto fallback;

    // 获取允许的大页 order 列表
    orders = thp_vma_allowable_orders(vma, vma->vm_flags,
                                      TVA_IN_PF | TVA_ENFORCE_SYSFS,
                                      BIT(PMD_ORDER) - 1);
    orders = thp_vma_suitable_orders(vma, vmf->address, orders);

    if (!orders)
        goto fallback;

    // 检查 PTE 范围是否全为空
    pte = pte_offset_map(vmf->pmd, vmf->address & PMD_MASK);
    if (!pte)
        return ERR_PTR(-EAGAIN);

    order = highest_order(orders);
    while (orders) {
        addr = ALIGN_DOWN(vmf->address, PAGE_SIZE << order);
        if (pte_range_none(pte + pte_index(addr), 1 << order))
            break;
        order = next_order(&orders, order);
    }

    pte_unmap(pte);

    if (!orders)
        goto fallback;

    // 尝试分配最大的可用 order
    gfp = vma_thp_gfp_mask(vma);
    while (orders) {
        addr = ALIGN_DOWN(vmf->address, PAGE_SIZE << order);

        // 分配 folio（可能是 2^order 个连续页）
        folio = vma_alloc_folio(gfp, order, vma, addr);

        if (folio) {
            // 计入 cgroup
            if (mem_cgroup_charge(folio, vma->vm_mm, gfp)) {
                count_mthp_stat(order, MTHP_STAT_ANON_FAULT_FALLBACK_CHARGE);
                folio_put(folio);
                goto next;
            }
            folio_throttle_swaprate(folio, gfp);

            // 清零页面
            if (user_alloc_needs_zeroing())
                folio_zero_user(folio, vmf->address);

            return folio;
        }
next:
        count_mthp_stat(order, MTHP_STAT_ANON_FAULT_FALLBACK);
        order = next_order(&orders, order);
    }

fallback:
#endif
    // 降级到普通单页分配
    return folio_prealloc(vma->vm_mm, vma, vmf->address, true);
}
```

**关键点**:
1. **尝试大页分配**: 优先尝试分配 THP (Transparent Huge Pages)
2. **降级机制**: 如果大页分配失败，降级到普通 4KB 页
3. **清零**: `folio_zero_user()` 确保新页内容为零（安全性）
4. **计费**: `mem_cgroup_charge()` 计入 cgroup 内存限制

---

## 总结：完整流程

```
用户程序访问未映射地址
         ↓
    [CPU 触发 #PF]
         ↓
① exc_page_fault()           ← 异常入口，读取 CR2
         ↓
② handle_page_fault()        ← 判断内核/用户地址
         ↓
③ do_user_addr_fault()       ← 用户地址处理，查找 VMA
         ↓
④ handle_mm_fault()          ← 通用 MM 处理
         ↓
⑤ __handle_mm_fault()        ← 页表遍历 (PGD→P4D→PUD→PMD)
         ↓
⑥ handle_pte_fault()         ← PTE 级别处理
         ↓
⑦ do_pte_missing()           ← 判断匿名/文件页
         ↓
⑧ do_anonymous_page()        ← 匿名页处理
         ↓
⑨ alloc_anon_folio()         ← 分配物理页
         ↓
   vma_alloc_folio()         ← 实际分配（Buddy System）
         ↓
   folio_zero_user()         ← 清零页面
         ↓
   folio_mk_pte()            ← 创建 PTE 条目
         ↓
   set_ptes()                ← 设置页表
         ↓
   update_mmu_cache_range()  ← 刷新 TLB
         ↓
    [返回用户态]
         ↓
   指令重新执行，成功访问内存 ✓
```

## 关键数据结构

### struct vm_fault
```c
struct vm_fault {
    struct vm_area_struct *vma;   // 出错的 VMA
    unsigned long address;         // 出错地址（页对齐）
    unsigned long real_address;    // 真实出错地址
    unsigned int flags;            // FAULT_FLAG_*
    pmd_t *pmd;                    // PMD 指针
    pte_t *pte;                    // PTE 指针
    pte_t orig_pte;                // 原始 PTE 值
    spinlock_t *ptl;               // 页表锁
    // ...
};
```

### folio (替代 struct page)
```c
// Folio 是一组连续的物理页（order 个页）
struct folio {
    unsigned long flags;           // 页标志
    atomic_t _refcount;            // 引用计数
    atomic_t _mapcount;            // 映射计数
    // ...
};
```

## 优化机制

### 1. 零页优化（Zero Page）
- 首次只读访问 → 映射到全局共享零页
- 首次写入时才分配真实物理页（延迟分配）

### 2. 透明大页（THP）
- 尝试分配 2MB 大页而非 4KB 小页
- 减少 TLB miss 和页表开销
- 失败时自动降级到 4KB 页

### 3. 写时复制（COW）
- fork 后父子进程共享只读页
- 写入时才复制（`do_wp_page()`）

### 4. 页面回收（LRU）
- 新分配的页加入 LRU 链表
- 内存紧张时从 LRU 尾部回收

## 性能影响

| 操作 | 首次访问开销 | 后续访问开销 |
|------|-------------|-------------|
| **栈/堆分配** | Page Fault + 物理页分配 | 0（已映射） |
| **只读访问** | Page Fault + 映射零页 | 0 |
| **写入零页** | Page Fault + COW + 分配 | 0 |
| **换入页面** | Page Fault + 磁盘 I/O | 0 |

**首次缺页开销**: ~1-2 微秒（不含磁盘 I/O）
- 页表遍历: ~100 ns
- 物理页分配: ~500 ns
- 页表设置: ~100 ns
- TLB 刷新: ~100 ns
- 系统调用切换: ~100 ns

这就是为什么：
- **栈分配快**: 通常栈页已预先映射（初始栈）
- **堆分配慢**: malloc 首次访问触发缺页
- **预分配有效**: 一次性分配并访问，后续无开销
