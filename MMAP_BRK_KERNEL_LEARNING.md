# 学习文档：mmap / brk / sbrk 与 Linux 内核实现

本文档说明 **mmap**、**brk**、**sbrk** 在 Linux 中的角色：哪些是内核系统调用、哪些是用户态 API，以及在内核源码中的定义与实现位置。结合本项目的 `heap_bench` 与 `strace` 输出可加深理解。

---

## 1. 结论速览

| 名称   | 是否在内核中实现 | 说明 |
|--------|------------------|------|
| **mmap** | 是 | 系统调用，内核中有完整实现；用户态 libc 的 `mmap()` 是对该 syscall 的封装。 |
| **brk**  | 是 | 系统调用，内核中有完整实现；用于设置进程的 program break（堆顶）。 |
| **sbrk** | 否 | **不是**系统调用，仅由 C 库（glibc、musl 等）在用户态实现，内部通过调用 **brk** 完成。 |

因此：**“mmap 是 kernel 调用吗？”** —— 是的，strace 里看到的 `mmap(...)` 就是进入内核的 **系统调用**；**brk** 同理。**sbrk** 不会直接出现在 syscall 列表里，它只是用户态对 brk 的封装。

---

## 2. 系统调用与用户态 API 的关系

- **系统调用（syscall）**：用户态通过软中断/`syscall` 指令进入内核，由内核执行并返回。`strace` 抓到的就是这些调用。
- **用户态 API**：C 库提供的函数（如 `malloc`、`mmap`、`sbrk`）。它们内部可能调用一个或多个 syscall（如 `brk`、`mmap`），但 **sbrk 本身不是 syscall**，只是对 **brk** 的封装。

```
用户态程序
    → 调用 libc 的 mmap() / sbrk()
        → mmap() 直接对应 syscall mmap
        → sbrk() 内部调用 syscall brk（并可能缓存当前 break）
    → 内核执行 sys_brk / ksys_mmap_pgoff 等
```

---

## 3. mmap 在内核中的定义与实现

### 3.1 通用实现（所有架构共用）

**文件**：`mm/mmap.c`（Linux 内核源码根目录下）

- **`ksys_mmap_pgoff()`**（约 569–611 行）  
  - 处理 `MAP_ANONYMOUS`、文件映射、huge page 等；
  - 最终调用 `vm_mmap_pgoff()` 完成虚拟内存映射。

- **`SYSCALL_DEFINE6(mmap_pgoff, ...)`**（约 613–618 行）  
  - 系统调用入口之一，直接调用 `ksys_mmap_pgoff()`。

- **`SYSCALL_DEFINE1(old_mmap, ...)`**  
  - 兼容旧 ABI 的 mmap 入口。

### 3.2 x86_64 的 mmap 系统调用入口

**文件**：`arch/x86/kernel/sys_x86_64.c`（约 82–90 行）

```c
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
                unsigned long, prot, unsigned long, flags,
                unsigned long, fd, unsigned long, off)
{
    if (off & ~PAGE_MASK)
        return -EINVAL;
    return ksys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
}
```

- 将字节偏移 `off` 转为页偏移后，调用通用函数 **`ksys_mmap_pgoff()`**。
- **`SYSCALL_DEFINE6(mmap, ...)`** 展开后函数名为 **`sys_mmap`**，这就是 syscall 表里挂的入口。

### 3.3 系统调用表（x86_64）

**文件**：`arch/x86/entry/syscalls/syscall_64.tbl`

- 第 **9** 号：`mmap` → **`sys_mmap`**（即上面 `sys_x86_64.c` 中的实现）。

---

## 4. brk 在内核中的定义与实现

### 4.1 实现位置与逻辑概要

**文件**：`mm/mmap.c`（约 115–215 行）

- **`SYSCALL_DEFINE1(brk, unsigned long, brk)`**
  - 参数：新的 program break 地址（堆顶）。
  - 返回值：成功时返回新的 break（即传入的 `brk`），失败时返回当前 break（`origbrk`）。

主要逻辑：

1. 加写锁 `mmap_write_lock(mm)`，取当前 `mm->brk` 为 `origbrk`。
2. 计算 `min_brk`（受 `CONFIG_COMPAT_BRK`、`brk_randomized` 等影响），若 `brk < min_brk` 则跳到 `out` 返回 `origbrk`。
3. 检查 `RLIMIT_DATA`，页对齐得到 `newbrk`、`oldbrk`。
4. 若 `oldbrk == newbrk`：只更新 `mm->brk = brk` 并返回。
5. **缩小堆**（`brk <= mm->brk`）：在 `[newbrk, oldbrk)` 做 `do_vmi_align_munmap`，更新 `mm->brk`。
6. **扩大堆**：检查与栈的 guard gap、调用 **`do_brk_flags()`** 扩展堆 VMA，再更新 `mm->brk`。
7. 失败路径 `out`：恢复 `mm->brk = origbrk`，返回 `origbrk`。

### 4.2 系统调用表（x86_64）

**文件**：`arch/x86/entry/syscalls/syscall_64.tbl`

- 第 **12** 号：`brk` → **`sys_brk`**（即 `mm/mmap.c` 中 `SYSCALL_DEFINE1(brk, ...)` 展开后的名字）。

---

## 5. sbrk：为何不在内核里？

- 内核只提供 **一个** 与“堆顶”相关的系统调用：**brk**，语义是“把 program break 设为某地址”，并返回当前 break。
- **sbrk(increment)** 的语义是“把 break 增加 increment”：
  - 需要“当前 break”才能计算“新 break = 当前 + increment”；
  - 当前 break 可由一次 `brk(0)` 或库内缓存得到；
  - 因此 **sbrk** 完全可以在用户态用 **brk** 实现，不需要单独的系统调用。

在 kernel 源码中搜索 **sbrk** 不会得到任何系统调用或实现；它只存在于 **C 库**（如 glibc、musl）中，是对 **brk** 的封装。

---

## 6. 与本项目的对应：strace 中的 mmap / brk

对 `heap_bench` 做：

```bash
strace -e trace=brk,mmap ./heap_bench
```

典型输出会包含：

- **brk(NULL)** / **brk(addr)**：C 库在初始化堆或扩展堆时调用的 **系统调用 brk**。
- **mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)**：分配匿名页时的 **系统调用 mmap**（例如 glibc malloc 对大块或某些策略下使用 mmap）。

每一行都是一次 **进入内核** 的 syscall；其在内核中的执行路径就是上文中的 **`sys_brk`**（`mm/mmap.c`）和 **`sys_mmap`** → **`ksys_mmap_pgoff`**（`mm/mmap.c` + `arch/x86/kernel/sys_x86_64.c`）。

---

## 7. 如何在本机对照内核源码阅读

若本机有 Linux 内核源码（例如 `~/works/linux`），可按下表定位：

| 内容           | 文件路径（相对于内核源码根目录） |
|----------------|----------------------------------|
| brk 实现       | `mm/mmap.c`，搜索 `SYSCALL_DEFINE1(brk` |
| mmap 通用逻辑  | `mm/mmap.c`，搜索 `ksys_mmap_pgoff`、`SYSCALL_DEFINE6(mmap_pgoff` |
| x86_64 mmap 入口 | `arch/x86/kernel/sys_x86_64.c`，搜索 `SYSCALL_DEFINE6(mmap` |
| x86_64 系统调用号 | `arch/x86/entry/syscalls/syscall_64.tbl`，查找 `mmap`、`brk` |

结合 **VERIFICATION_REPORT.md**、**PERFORMANCE_BREAKDOWN.md** 中的 strace/perf 结果，可以对照“用户态 malloc 行为 → syscall 次数 → 内核 brk/mmap 实现”做一条龙学习。

---

## 8. 参考

- Linux 内核源码：`mm/mmap.c`、`arch/x86/kernel/sys_x86_64.c`、`arch/x86/entry/syscalls/syscall_64.tbl`
- 本项目：**VERIFICATION_REPORT.md**（strace 结果）、**WHY_SO_DIFFERENT.md**（栈与堆差异）
- man 手册：`man 2 brk`、`man 2 mmap`、`man 3 sbrk`（注意 2 = syscall，3 = libc）
