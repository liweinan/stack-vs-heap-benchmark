# 容器 Linux（Alpine / GCC / aarch64-musl）下 `heap_vs_stack_fairness` 汇编分析

本文档基于 **项目 Docker 镜像内** 的实际编译产物：在容器中对 `demos/heap_vs_stack_fairness.c` 使用 **`gcc -S -O2`**（默认启用栈保护）生成汇编，并结合 **`readelf` / `nm` / `strace`** 说明与内核的边界。

**复现命令（在仓库根目录）：**

```bash
docker-compose run --rm benchmark sh -c \
  'gcc --version; gcc -S -O2 -fverbose-asm demos/heap_vs_stack_fairness.c -o /tmp/h.s'
```

容器内典型输出：**GCC (Alpine 15.2.0) 15.2.0**，目标为 **ELF64 aarch64**，动态链接 **musl**（`libc.musl-aarch64.so.1` 与 `/lib/ld-musl-aarch64.so.1` 为同一 musl 提供方）。

---

## 1. 场景 2：为何「堆复用」路径可能快于「栈 + 每次调用」

源码意图：栈侧在循环内 **`bl process_with_stack_buffer`**；堆侧 **`malloc` 在第一次 `clock_gettime` 之前**，计时区间内只有对堆缓冲区的循环访问，`free` 在第二次 `clock_gettime` 之后。

### 1.1 栈侧：外层循环（`test2_stack_with_function_call`）

计时开始后，核心为 **百万次带链接跳转**：

```asm
.L49:
    mov     w0, w19
    add     w19, w19, #1
    bl      process_with_stack_buffer
    cmp     w19, w20
    bne     .L49
```

每次迭代至少包含 **`bl` + 被调函数序言/尾声**（**`bl` 含义与 x86-64 `call` 的对照见 §1.4**）。这与「栈寻址比堆寻址快」无直接对应：瓶颈在 **调用约定与控制流**，而非 `sp` 相对寻址本身。

### 1.2 栈侧：被调函数（`process_with_stack_buffer`）

在 **`-fstack-protector`（默认）** 下，被调函数为 **1024 字节局部数组** 分配 **大栈帧**，并执行 **金丝雀存取与校验**：

```asm
    stp     x29, x30, [sp, #-16]!
    adrp    x1, :got:__stack_chk_guard
    ldr     x1, [x1, :got_lo12:__stack_chk_guard]
    mov     x29, sp
    sub     sp, sp, #1040          ; 1024 + 金丝雀槽位等
    ...
    strb    w0, [sp, #8]
    ...
    strb    w0, [sp, #1031]
    ...
    ; 返回前：金丝雀比较，失败则 bl __stack_chk_fail
    add     sp, sp, #1040
    ldp     x29, x30, [sp], #16
    ret
```

要点：

- **`sub sp, sp, #1040` / `add sp, sp, #1040`** 在每轮调用中各执行一次：为局部 `buffer[1024]` 预留与回收栈空间。
- **每轮调用两次触及 `__stack_chk_guard`**（序言中写入金丝雀副本、尾声前与全局 canary 比较），经 **GOT** 访问用户态全局，**不进入内核**；**对应 `adrp`/`ldr`/`str`/`subs` 等真实指令见 §2.4**。
- 热路径上的实际访存与堆侧类似（首尾字节 + `g_sum`），额外成本主要来自 **`bl`/`ret`、栈帧调整、栈保护指令序列**。

### 1.3 堆侧：计时区间内的内层循环（`test2_heap_reuse`）

`malloc` 在 **`clock_gettime`（start）之前** 执行；计时区间内 **`x19` 保存堆指针**，循环为 **直线代码**，无 `bl` 至 `process_with_stack_buffer`：

```asm
    ; malloc、第一次 clock_gettime 之后进入循环
.L21:
    strb    w2, [x19]
    asr     w0, w2, #8
    strb    w0, [x19, #1023]
    add     w2, w2, #1
    ldrb    w1, [x19]
    ldrb    w0, [x19, #1023]
    ...
    cmp     w2, w5
    bne     .L21
    ; 第二次 clock_gettime 之后才是 free
```

要点：

- 计时区间内 **无** 每轮 `sub sp, #1040`、**无** 每轮 `bl`/`ret`、**无** 子函数内的 **每轮** 栈金丝雀序列（仅 `test2_heap_reuse` 自身函数级序言/尾声各一次）。
- 因此场景 2 若测得堆侧更快，汇编层面解释为：**比较的是「百万次完整调用 + 大栈帧 + 栈保护」与「百万次紧循环 + 寄存器持有堆指针」**，而非「内核里栈比堆慢」。

### 1.4 背景：AArch64 的 `bl` / `ret` 与 x86-64 的 `call` / `ret`

若更熟悉 Intel 语法，可把 **`bl`** 理解成「带保存返回地址的跳转」，与 x86-64 的 **`call`** 同属**函数调用**指令；差别主要在**返回地址放在哪里**。

**AArch64：`BL label`（Branch with Link）**

- **Link** 指 CPU 把**下一条指令地址**（当前 PC+4）写入 **链接寄存器 `X30`**（汇编里常写作 **`LR`**），再跳转到 `label`。
- 语义上就是：**调用子程序**：返回点保存在 **寄存器**里，而不是先压栈（除非被调函数要自己再 `stp x29,x30,[sp,#-16]!` 把 **LR/FP 存到栈上**——叶函数若不再调用别人，有时可不碰栈）。
- 典型返回指令 **`ret`**（等价于 **`ret x30`**）：跳转到 **LR** 中的地址。若 LR 已被存入栈，则先 **`ldp x29,x30,[sp],#16`** 再 **`ret`**。

**x86-64：`call` / `ret`（System V ABI 下常见形态）**

- **`call rel` / `call *r`**：把**返回地址**（下一条指令的 **RIP**）**压入当前栈顶**，再跳转到目标。返回地址在**内存（栈）**里。
- **`ret`**：从栈顶**弹出**返回地址并跳转（可带立即数调整 `rsp`，用于清理参数）。

**简要对照**

| 概念 | AArch64（本文档容器） | x86-64（典型 GNU/LLVM） |
|------|------------------------|-------------------------|
| 调用 | `bl foo` | `call foo` |
| 返回 | `ret`（用 LR / x30） | `ret` |
| 返回地址默认保存在 | **寄存器 x30（LR）** | **栈顶**（由 `call` 压入） |

**与本文测量的关系**：场景 2 栈路径里每轮 **`bl` + 被调函数末尾 `ret`**，与 x86-64 上每轮 **`call` + `ret`** 一样，都会引入**控制流转移**以及（在需要保存 LR/帧指针时）**额外的访存**；本仓库汇编为 **aarch64**，故文中写 **`bl`**。若在 x86-64 上编译同一 C 代码，对应位置会是 **`call`**，而不是同名的 **`bl`**。

---

## 2. `__stack_chk_guard` 与内核的关系

### 2.1 符号落在何处（容器内 musl）

对 musl 动态链接器/ libc 映像（`/lib/ld-musl-aarch64.so.1`）可查：

- `__stack_chk_guard`：**类型为 `OBJECT`，位于 `.bss`（8 字节）**，由 **用户态** musl 映像提供。
- `__stack_chk_fail`：**用户态函数**（`.text`）。

可执行文件中对 `__stack_chk_guard` 为 **`UND` 未定义引用**，运行时由上述 DSO 解析；**不是内核导出的符号**，也**不是** vDSO 接口。

### 2.2 随机性从哪里来

进程由 **`execve` 创建**时，内核在 **辅助向量 auxv** 中提供 **`AT_RANDOM`**（指向 16 字节随机数据的用户态指针）。**ld.so / libc 启动代码** 使用该数据（及平台约定）**初始化** `__stack_chk_guard`。**内核提供的是经 auxv 传递的熵与进程地址空间布局；维护与检查 canary 的逻辑在用户态。**

### 2.3 与「内核分析」的边界

场景 2 计时循环内，**栈保护相关指令均为用户态**：GOT 加载、`ldr`/`str`、比较、`__stack_chk_fail`。**不因读取 canary 而产生单独的系统调用。**

### 2.4 `__stack_chk_guard` 对应的实际汇编（容器 GCC / aarch64）

下列指令摘自仓库内完整清单 **`demos/heap_vs_stack_fairness.linux_container_O2.s`** 中 **`process_with_stack_buffer`**（场景 2 栈路径每轮都会进入该函数一次）。

**序言：经 GOT 取得全局 `__stack_chk_guard` 的地址，再读出 8 字节 canary，写入栈上副本槽 `[sp, #1032]`（与 `sub sp, sp, #1040` 配套）。**

```asm
    adrp    x1, :got:__stack_chk_guard
    ldr     x1, [x1, :got_lo12:__stack_chk_guard]   ; x1 ← &guard（GOT 项）
    mov     x29, sp
    sub     sp, sp, #1040
    ldr     x2, [x1]                                 ; x2 ← *guard（当前金丝雀值）
    str     x2, [sp, 1032]                           ; 栈上保存副本，供返回前比对
```

**尾声：再次经 GOT 取 `__stack_chk_guard`，将栈上副本与内存中的当前值比较；不一致则跳转到 `__stack_chk_fail`。**

```asm
    adrp    x0, :got:__stack_chk_guard
    ldr     x0, [x0, :got_lo12:__stack_chk_guard]
    ldr     x2, [sp, 1032]          ; 调用开始时存入的副本
    ldr     x1, [x0]                 ; 再次读取 *guard
    subs    x2, x2, x1
    bne     .L47                     ; 不相等 → .L47
    add     sp, sp, #1040
    ldp     x29, x30, [sp], #16
    ret
.L47:
    bl      __stack_chk_fail
```

要点：**`adrp`/`ldr` 配对访问的是 GOT**，最终读到的是 musl 提供的 **`__stack_chk_guard` 变量**（见 §2.1）；**`ldr x2,[x1]` / `ldr x1,[x0]`** 才是对 **canary 数值** 的加载。其它带大栈帧的函数（如 **`test1_stack_small_object`**、**`test2_stack_with_function_call`**）在函数入口/出口也使用同一模式，仅 **栈槽偏移**（如 `[sp, #1064]` 与 `#1072`）与寄存器分配可能不同。

---

## 3. 系统调用视角（`strace`）

对整程序运行 **`strace -c`** 时，常见情况是 **`mmap` / `munmap` 占比较高**——对应 **`malloc`/`free` 及分配器向内核申请/归还映射**（场景 1、3、4 及堆路径会放大）。**场景 2 的计时核心**若仅覆盖「堆缓冲区的循环」，则 **不** 在计时区间内重复 `mmap`；栈侧亦无 syscall 级「栈保护」。

因此：**汇编级差异主要在用户态指令与函数调用**；内核侧在整程序层面体现为 **堆管理相关的 `mmap`/`munmap`（及调度、缺页等）**，与场景 2 热路径的对比需分开讨论。

---

## 4. 与其他环境的差异（避免误读）

在 **macOS + Clang** 上，同一源码可能将 `process_with_stack_buffer` **内联**进调用方，甚至收缩未使用的栈空间，使场景 2 的对比 **不再等价于**「百万次 `bl`」。**以容器内 Linux GCC 生成的汇编为准**时，栈侧保留 **`bl process_with_stack_buffer`**，与本文分析一致。

若需对照「关闭栈保护」的代码生成，可在容器内显式使用 **`-fno-stack-protector`** 重新生成 `.s` 比较（仅用于实验，生产环境需权衡安全性）。

---

## 5. 完整汇编清单（带标注）

仓库内 **`demos/heap_vs_stack_fairness.linux_container_O2.s`** 为容器内 **完整** `gcc -S -O2 -fverbose-asm` 输出，并在此基础上用 **`/* … */`** 增加了块注释与 **`【】`** 标出的要点（各测试函数、`__stack_chk_guard` 相关序言/尾声、热循环标签 **`.L2` / `.L21` / `.L49`**、`main` / `run_test` / `g_sum` 等）。

重新生成无标注版本并覆盖本文件时：

```bash
docker-compose run --rm benchmark sh -c \
  'gcc -S -O2 -fverbose-asm demos/heap_vs_stack_fairness.c \
   -o demos/heap_vs_stack_fairness.linux_container_O2.s'
```

校验「带标注汇编可被汇编器接受」：

```bash
docker-compose run --rm benchmark sh -c \
  'gcc -c demos/heap_vs_stack_fairness.linux_container_O2.s -o /tmp/o.o'
```

---

## 6. 二进制与栈保护符号：实用命令（容器内）

对 **`./heap_vs_stack_fairness`**（在仓库根目录构建或容器 `/workspace` 下）：

```bash
# 动态符号：是否引用 guard / fail（U = 未解析，由 libc 提供）
nm -D ./heap_vs_stack_fairness | grep stack_chk

readelf -Ws ./heap_vs_stack_fairness | grep stack_chk

# 重定位：GOT 数据槽 vs PLT 函数槽
objdump -R ./heap_vs_stack_fairness | grep stack_chk
```

**说明**：

- **`__stack_chk_guard`**：类型为 **OBJECT**，热路径是 **`adrp`/`ldr` 读数据**，不是 `bl`「调用」。
- **`__stack_chk_fail`**：类型为 **FUNC**，反汇编里常出现 **`__stack_chk_fail@plt`**；**仅 canary 校验失败** 时才会 `bl` 进入，正常路径不执行。
- **`objdump -D ... | grep stack_chk_guard` 经常无输出**：反汇编行未必包含该字符串；用 **`nm`/`readelf`/`objdump -R`** 更可靠（详见 **`MUSL_STACK_CHK_GUARD_ANALYSIS.md` §8**）。

**带重定位的反汇编**：

```bash
objdump -d -r ./heap_vs_stack_fairness | less
```

**统计 strace（整程序 syscall 分布，非场景 2 独占）**：

```bash
strace -c ./heap_vs_stack_fairness
```

---

## 7. 参考

- AArch64 **`bl`/`ret`** 与 x86-64 **`call`/`ret`**：**§1.4**。
- 源码：`demos/heap_vs_stack_fairness.c`（场景 2：`test2_stack_with_function_call` / `test2_heap_reuse` / `process_with_stack_buffer`）。
- 生成汇编：`gcc -S -O2 -fverbose-asm demos/heap_vs_stack_fairness.c`。
- 完整带标注列表：`demos/heap_vs_stack_fairness.linux_container_O2.s`。
- musl 中 **`__stack_chk_guard` 初始化**、**动态符号与 `objdump`/`nm` 实用命令**：**`MUSL_STACK_CHK_GUARD_ANALYSIS.md`**（§7–§9）。
- 场景 2 与 **缓存**：**`SCENARIO2_CACHE_NOTE.md`**。
