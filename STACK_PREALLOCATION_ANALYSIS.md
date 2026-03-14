# 栈预分配分析报告

## 问题背景

我们的测试程序 `stack_growth_comparison` 预期产生 ~604 次缺页中断：
- 场景 1: ~4 次（首次访问 4 页）
- 场景 2: ~400 次（100 层递归 × 4 页/层）
- 场景 3: ~200 次（首次 50 层 × 4 页，后续复用）

**实际结果**: 只有 25-27 次缺页中断

## 内核源码分析

### 1. 进程启动时的栈初始化

#### 阶段 1: 创建初始栈 VMA (1 页)

**文件**: `/Users/weli/works/linux/mm/vma_exec.c:139`

```c
int create_init_stack_vma(struct mm_struct *mm, struct vm_area_struct **vmap,
                          unsigned long *top_mem_p)
{
    // ...
    vma->vm_end = STACK_TOP_MAX;
    vma->vm_start = vma->vm_end - PAGE_SIZE;  // 只创建 1 页 VMA
    // ...
}
```

- **时机**: `bprm_mm_init()` 调用，在加载 ELF 之前
- **大小**: 仅 1 页（4KB）
- **作用**: 创建临时栈 VMA，用于后续扩展

#### 阶段 2: 扩展栈 VMA 到 128KB

**文件**: `/Users/weli/works/linux/fs/exec.c:698-714`

```c
int setup_arg_pages(struct linux_binprm *bprm, ...)
{
    // ...
    stack_expand = 131072UL;  /* randomly 32*4k (or 2*64k) pages */
                              /* 128 KB = 32 页 */
    stack_size = vma->vm_end - vma->vm_start;

    rlim_stack = bprm->rlim_stack.rlim_cur & PAGE_MASK;
    stack_expand = min(rlim_stack, stack_size + stack_expand);

#ifdef CONFIG_STACK_GROWSUP
    stack_base = vma->vm_start + stack_expand;
#else
    stack_base = vma->vm_end - stack_expand;  // 向下增长
#endif

    ret = expand_stack_locked(vma, stack_base);  // 扩展 VMA 到 128KB
    // ...
}
```

- **时机**: `load_elf_binary()` 调用，在设置 personality 之后
- **大小**: 128 KB（32 页）
- **重要**: `expand_stack_locked()` **只扩展 VMA 边界，不分配物理页**

#### 阶段 3: VMA 扩展的实现

**文件**: `/Users/weli/works/linux/mm/vma.c:3076-3082`

```c
int expand_downwards(struct vm_area_struct *vma, unsigned long address)
{
    // ...
    error = acct_stack_growth(vma, size, grow);  // 只做账户检查
    if (!error) {
        if (vma->vm_flags & VM_LOCKED)
            mm->locked_vm += grow;
        vm_stat_account(mm, vma->vm_flags, grow);
        anon_vma_interval_tree_pre_update_vma(vma);
        vma->vm_start = address;  // 仅修改 VMA 起始地址
        anon_vma_interval_tree_post_update_vma(vma);
        // ...
    }
    // ...
}
```

**`acct_stack_growth()` 函数** (`/Users/weli/works/linux/mm/vma.c:2898-2930`):
```c
static int acct_stack_growth(struct vm_area_struct *vma,
                             unsigned long size, unsigned long grow)
{
    struct mm_struct *mm = vma->vm_mm;

    /* address space limit tests */
    if (!may_expand_vm(mm, vma->vm_flags, grow))
        return -ENOMEM;

    /* Stack limit test */
    if (size > rlimit(RLIMIT_STACK))
        return -ENOMEM;

    /* mlock limit tests */
    if (!mlock_future_ok(mm, vma->vm_flags, grow << PAGE_SHIFT))
        return -ENOMEM;

    // ... security checks ...

    return 0;  // 只做检查，不分配物理页
}
```

**关键发现**:
- ✅ VMA 在启动时扩展到 128KB
- ❌ **物理页不会预先分配**（除非设置了 `VM_LOCKED` 标志）
- ✅ 页面在首次访问时通过缺页中断按需分配

### 2. 仅当栈被锁定时才预先分配

**文件**: `/Users/weli/works/linux/mm/mmap.c:1002-1003`

```c
struct vm_area_struct *find_extend_vma_locked(struct mm_struct *mm, unsigned long addr)
{
    // ...
    if (expand_stack_locked(vma, addr))
        return NULL;
    if (vma->vm_flags & VM_LOCKED)
        populate_vma_page_range(vma, addr, start, NULL);  // 预先分配物理页
    return vma;
}
```

**结论**: 普通程序的栈**没有** `VM_LOCKED` 标志，所以物理页不会预先分配。

### 3. 启动时触发的栈访问

#### ELF 加载器写入栈数据

**文件**: `/Users/weli/works/linux/fs/binfmt_elf.c:149-348`

`create_elf_tables()` 函数将以下数据写入栈：

1. **平台字符串** (line 189-192):
```c
u_platform = (elf_addr_t __user *)STACK_ALLOC(p, len);
if (copy_to_user(u_platform, k_platform, len))  // 触发缺页
    return -EFAULT;
```

2. **随机字节** (line 210-214):
```c
get_random_bytes(k_rand_bytes, sizeof(k_rand_bytes));
u_rand_bytes = (elf_addr_t __user *)STACK_ALLOC(p, sizeof(k_rand_bytes));
if (copy_to_user(u_rand_bytes, k_rand_bytes, sizeof(k_rand_bytes)))  // 触发缺页
    return -EFAULT;
```

3. **Auxiliary Vector (auxv)** (line 218-276):
```c
NEW_AUX_ENT(AT_HWCAP, ELF_HWCAP);
NEW_AUX_ENT(AT_PAGESZ, ELF_EXEC_PAGESIZE);
NEW_AUX_ENT(AT_CLKTCK, CLOCKS_PER_SEC);
// ... 约 20 个 auxv 条目 ...
```

4. **参数和环境变量** (line 311-342):
```c
// 参数指针数组
while (argc-- > 0) {
    if (put_user((elf_addr_t)p, sp++))  // 触发缺页
        return -EFAULT;
    len = strnlen_user((void __user *)p, MAX_ARG_STRLEN);
    p += len;
}

// 环境变量指针数组
while (envc-- > 0) {
    if (put_user((elf_addr_t)p, sp++))  // 触发缺页
        return -EFAULT;
    len = strnlen_user((void __user *)p, MAX_ARG_STRLEN);
    p += len;
}

// auxv 复制到栈
if (copy_to_user(sp, mm->saved_auxv, ei_index * sizeof(elf_addr_t)))
    return -EFAULT;
```

5. **显式栈扩展** (line 303-308):
```c
if (mmap_write_lock_killable(mm))
    return -EINTR;
vma = find_extend_vma_locked(mm, bprm->p);  // 扩展 VMA 到包含 bprm->p
mmap_write_unlock(mm);
if (!vma)
    return -EFAULT;
```

#### 实际验证: Alpine 容器

```bash
$ docker run --rm alpine:latest cat /proc/self/maps | grep stack
ffffc7434000-ffffc7455000 rw-p 00000000 00:00 0  [stack]
```

计算结果：
- 起始: `0xffffc7434000`
- 结束: `0xffffc7455000`
- 大小: `135168 bytes = 132 KB = 33 页`

**分析**:
- VMA 大小 = 132 KB，略大于内核代码中的 128 KB（32 页）
- 额外的 1 页可能是：
  - Guard page（栈保护页）
  - 初始的 1 页 VMA
  - 实际测试中已经访问的页

## 编译代码分析

### objdump 反汇编结果

**函数**: `touch_stack_no_restore()` (src/stack_growth_comparison.c:71)

```asm
000000000000121b <touch_stack_no_restore>:
    121b:   push   %rbp
    121c:   mov    %rsp,%rbp
    121f:   sub    $0x40,%rsp          # 分配 64 字节局部变量
    1223:   mov    %edi,-0x34(%rbp)    # 保存参数 num_pages

    # ... stack canary 检查 ...

    123b:   mov    -0x34(%rbp),%eax    # eax = num_pages
    123e:   shl    $0xc,%eax           # eax = num_pages * 4096
    1241:   movslq %eax,%rdx
    1244:   sub    $0x1,%rdx

    # ... 对齐计算 ...

    1264:   div    %rdi
    1267:   imul   $0x10,%rax,%rax     # rax = aligned_size
    126b:   sub    %rax,%rsp           # ★ 关键: 分配栈空间 ★
    126e:   mov    %rsp,%rax
    1271:   mov    %rax,-0x18(%rbp)    # 保存 buffer 地址
```

**关键行**: `126b: sub %rax,%rsp`
- 这是栈分配的核心指令
- `%rax` = `num_pages * 4096` (对齐后)
- 执行后 RSP 向下移动，分配栈空间

### 访问模式分析

**循环写入每一页** (汇编地址 `1284-12c1`):

```asm
    127d:   movl   $0x0,-0x24(%rbp)    # i = 0
    1284:   jmp    1315                # 跳到循环条件

1289:   # 写入页首: p[i * PAGE_SIZE] = 0x42 + i
    1289:   mov    -0x24(%rbp),%eax    # eax = i
    128c:   lea    0x42(%rax),%ecx     # ecx = 0x42 + i
    128f:   mov    -0x24(%rbp),%eax
    1292:   shl    $0xc,%eax           # eax = i * 4096
    1295:   movslq %eax,%rdx
    1298:   mov    -0x10(%rbp),%rax    # rax = buffer
    129c:   add    %rdx,%rax           # rax = buffer + i*4096
    129f:   mov    %ecx,%edx
    12a1:   mov    %dl,(%rax)          # ★ 写入页首 ★

    # 写入页尾: p[i * PAGE_SIZE + 4095] = 0x42 + i
    12a3:   mov    -0x24(%rbp),%eax
    12a6:   lea    0x42(%rax),%ecx
    12a9:   mov    -0x24(%rbp),%eax
    12ac:   shl    $0xc,%eax
    12af:   cltq
    12b1:   lea    0xfff(%rax),%rdx    # rdx = i*4096 + 4095
    12b8:   mov    -0x10(%rbp),%rax
    12bc:   add    %rdx,%rax
    12bf:   mov    %ecx,%edx
    12c1:   mov    %dl,(%rax)          # ★ 写入页尾 ★
```

**验证**:
- ✅ 编译器正确生成了 `sub %rax,%rsp` 指令来分配栈空间
- ✅ 每一页的首尾都被显式访问（`mov %dl,(%rax)`）
- ✅ 使用 `-O0` 编译，没有优化干扰
- ✅ 使用 `volatile` 指针防止编译器优化掉访问

**结论**: 编译后的代码行为符合预期，应该触发缺页中断。

## 为什么只有 25-27 次缺页？

### 假设 1: 启动时已访问的栈页

**启动阶段触发的缺页**:

1. **ELF 加载器写入栈** (create_elf_tables):
   - 参数字符串: `./stack_growth_comparison`
   - 环境变量: `PATH`, `HOME`, `TERM`, 等
   - Auxiliary vector: ~20 个条目 × 16 字节 = 320 字节
   - 平台字符串、随机字节等

2. **C 运行时初始化**:
   - `__libc_start_main()` 的栈帧
   - 全局变量初始化
   - 线程本地存储（TLS）

3. **main() 函数开始执行前**:
   - `printf()` 打印初始信息
   - `clock_gettime()` 调用
   - 栈上的局部变量

**估算**:
- 假设启动时访问了 5-8 页（20-32 KB）
- 这些页在后续测试中会被复用

### 假设 2: 测试场景之间的栈页复用

**场景 1: 固定深度重复调用**
- 配置: 100 次调用，每次 4 页（16KB）
- 预期缺页: 首次 4 次，后续 0 次
- **实际**: 如果这 4 页在启动时已被访问，则缺页 = 0

**场景 2: 持续增长递归深度**
- 配置: 递归 100 层，每层 4 页
- 预期缺页: 每层 4 次，总计 400 次
- **实际**: 如果栈深度不够，可能只触发部分缺页

**场景 3: 重复递归**
- 配置: 50 层递归，重复 10 次
- 预期缺页: 首次 200 次，后续 0 次
- **实际**: 复用场景 2 的栈页，缺页 = 0

### 假设 3: 实际栈增长小于预期

**验证方法**: 在容器中运行并追踪栈深度

```bash
# 使用 perf 追踪实际缺页次数
docker run --rm --privileged alpine:latest sh -c '
  apk add linux-tools-generic  # 安装 perf
  ./stack_growth_comparison &
  PID=$!
  perf stat -e page-faults -p $PID
'
```

**可能原因**:
1. **递归深度不够**: 100 层 × 16KB = 1.6MB，但实际栈可能没有增长这么多
2. **栈帧被优化**: `-O0` 编译仍可能有基本优化
3. **页面共享**: 某些页面在不同调用间被复用

## 结论

### 内核行为（已确认）

1. ✅ **VMA 扩展**: 启动时 VMA 扩展到 128KB（32 页）
2. ✅ **惰性分配**: 物理页不会预先分配，只在首次访问时分配
3. ✅ **启动访问**: ELF 加载器和 C 运行时会访问部分栈页

### 缺页数不符的原因（推测）

基于以上分析，25-27 次缺页的可能原因：

1. **启动时预先访问**: 5-10 页被 ELF 加载器和运行时访问
2. **场景 1 复用**: 4 页在启动时已分配，缺页 = 0
3. **场景 2 部分增长**: 实际只增长了 10-15 页，而非 400 页
4. **场景 3 完全复用**: 复用场景 2 的栈页，缺页 = 0

**总缺页 = 启动 (5-10) + 场景1 (0) + 场景2 (10-15) + 场景3 (0) ≈ 15-25 次**

### 建议的验证方法

1. **追踪栈指针变化**: 在测试中打印每次递归的 RSP 值
2. **使用 strace**: 追踪 mmap/brk 系统调用
3. **使用 perf 记录**: `perf record -e page-faults` 记录每次缺页的调用栈
4. **增加递归深度**: 将场景 2 改为 1000 层，确保超出预分配范围

## 参考文件

- `/Users/weli/works/linux/fs/exec.c:698-714` - setup_arg_pages()
- `/Users/weli/works/linux/fs/binfmt_elf.c:149-348` - create_elf_tables()
- `/Users/weli/works/linux/mm/vma_exec.c:107-161` - create_init_stack_vma()
- `/Users/weli/works/linux/mm/vma.c:2898-2930` - acct_stack_growth()
- `/Users/weli/works/linux/mm/vma.c:3023-3082` - expand_downwards()
- `/Users/weli/works/linux/mm/mmap.c:983-1005` - expand_stack_locked()
