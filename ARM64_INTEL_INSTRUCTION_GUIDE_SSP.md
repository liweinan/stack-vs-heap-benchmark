# ARM64 与 Intel 指令对照学习笔记（基于场景2 guard 校验）

本文围绕 `demos/heap_vs_stack_fairness.linux_container_O2.s.md` 的场景2代码，解释你问到的 `.L53` 跳转原因，并给出 ARM64 与 x86-64（Intel）常见指令对照。

---

## 1. 先回答：为什么会跳到 `.L53`

对应片段：

```asm
adrp x1, :got:__stack_chk_guard
ldr  x1, [x1, :got_lo12:__stack_chk_guard]
ldr  x3, [sp, 40]
ldr  x2, [x1]
subs x3, x3, x2
bne  .L53
```

逻辑是：

1. 从 GOT 找到 `__stack_chk_guard`，读出当前 canary 到 `x2`。
2. 从栈帧里读出函数入口保存的副本到 `x3`。
3. `subs x3, x3, x2` 做减法并更新条件标志位（NZCV）。
4. `bne .L53`：**若不相等（Z=0）就跳转**到 `.L53`。
5. `.L53` 里是 `bl __stack_chk_fail`，进入崩溃路径。

所以 `.L53` 不是“随机跳转”，而是 guard 比较失败的固定异常出口。

---

## 2. 关键概念：`cmp` / `subs` 与 `bne`

在 ARM64 里，分支常依赖前一条“设置标志位”的指令。

- `subs a, b, c`：计算 `b-c`，结果写回 `a`，并更新标志位。
- `cmp b, c`：本质是 `subs xzr, b, c`（只设标志，不留结果）。
- `bne label`：Branch if Not Equal，条件是 **Z=0**。

对应 x86-64：

- `sub r1, r2` / `cmp r1, r2` 更新 EFLAGS。
- `jne label` 根据 ZF=0 跳转。

---

## 3. 本文件里常见 ARM64 ↔ x86-64 对照

| 语义 | ARM64 | x86-64（Intel） | 说明 |
|---|---|---|---|
| 函数调用 | `bl func` | `call func` | ARM64 把返回地址放 `x30(LR)`；x86-64 压栈返回地址 |
| 函数返回 | `ret` | `ret` | ARM64 默认从 `x30` 返回 |
| 比较后分支 | `subs/cmp` + `bne` | `cmp` + `jne` | 都依赖标志位 |
| 栈帧保存 | `stp x29, x30, [sp, -16]!` | `push rbp; mov rbp,rsp` | 保存帧指针/返回地址 |
| 栈帧恢复 | `ldp x29, x30, [sp], 16` | `leave` / `pop rbp` | 恢复现场 |
| 从内存加载 | `ldr xN, [base,off]` | `mov rN, [base+off]` | load |
| 写内存 | `str xN, [base,off]` | `mov [base+off], rN` | store |
| 成对访存 | `ldp/stp` | 两条 `mov`/`push`/`pop` | ARM64 常用以减指令数 |
| 页内地址拼接 | `adrp` + `add/ldr` | `lea` + RIP 相对寻址 | ARM64 用 page + low12 组合 |
| 乘加融合 | `madd x0,x1,x2,x3` | `imul` + `add`（或 FMA 类） | 整数 fused multiply-add |

---

## 4. guard 相关最小指令模板（ARM64）

在开启 SSP 的函数里，常见模板：

1) 入口保存 guard 副本

```asm
adrp xN, :got:__stack_chk_guard
ldr  xN, [xN, :got_lo12:__stack_chk_guard]
ldr  xM, [xN]
str  xM, [sp, SLOT]
```

2) 返回前校验

```asm
adrp xN, :got:__stack_chk_guard
ldr  xN, [xN, :got_lo12:__stack_chk_guard]
ldr  xA, [sp, SLOT]
ldr  xB, [xN]
subs xA, xA, xB
bne  .L_fail
```

3) 失败出口

```asm
.L_fail:
bl __stack_chk_fail
```

对应 x86-64 语义完全一致，只是实现细节不同（RIP 相对寻址、`cmp/jne`、返回地址保存在栈上）。

---

## 5. 结合你当前文件的定位建议

在 `demos/heap_vs_stack_fairness.linux_container_O2.s.md` 里，场景2 stack 路径重点看两段：

- `process_with_stack_buffer`：函数级 guard 入口保存 + 尾声比较 + `__stack_chk_fail` 分支。
- `test2_stack_with_function_call`：外层函数自己的 guard 校验（即你提到的 `.L53`）。

阅读顺序建议：

1. 先找 `subs` / `cmp`；
2. 看紧随其后的 `bne` / `beq`；
3. 再回看前面哪条 `ldr/str` 在准备比较双方；
4. 最后定位失败标签里是否 `bl __stack_chk_fail`。

---

## 6. 快速记忆（够用版）

- `bl` ~= `call`
- `ret` ~= `ret`
- `subs/cmp + bne` ~= `cmp + jne`
- `adrp+ldr` 常用于全局符号/GOT 访问
- guard 失败分支最终都会到 `__stack_chk_fail`

