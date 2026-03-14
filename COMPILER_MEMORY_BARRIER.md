# 编译器内存屏障说明

本文档说明项目中使用的 `__asm__ volatile("" : : : "memory");` 的含义、作用及与 Intel SDM 的对应关系。

## 代码形式

```c
__asm__ volatile("" : : : "memory");
```

## 语法拆解（GCC 扩展内联汇编）

| 部分 | 含义 |
|------|------|
| `__asm__` | 与 `asm` 等价；在 `-std=c11` 等模式下 `asm` 可能为保留字，故用 `__asm__` |
| `volatile` | 表示该 asm 有可观察副作用，编译器不得删除、合并或移出循环 |
| `""` | 汇编模板为空：**不生成任何机器指令** |
| 第一个 `:` | 无输出操作数 |
| 第二个 `:` | 无输入操作数 |
| 第三个 `:` 后的 `"memory"` | **破坏列表**：声明该 asm 可能读写任意可寻址内存 |

## 语义：编译器级内存栅栏

- **`"memory"` 在破坏列表中的效果**（参见 [GCC 文档 - Extended Asm - Clobbers](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)）：
  - 编译器假定该 asm 会读写任意内存；
  - 该 asm **之前**的所有写必须在 asm 之前完成（不能把写重排到 asm 之后）；
  - 该 asm **之后**的读不能使用 asm 之前的缓存，必须从内存（或当前可见状态）重新取；
  - 编译器不会把 asm 两侧的访存相互重排。

- **重要**：这是 **编译器内存屏障（compiler fence）**，不是 CPU 指令级屏障：
  - 不生成任何指令（如 x86 的 MFENCE/LFENCE/SFENCE）；
  - 只约束编译器的优化与重排，不约束 CPU 乱序或多核可见性。

## 在本项目中的用途

在 `src/stack_depth_tracer.c`、`src/stack_growth_comparison.c` 等文件中，该屏障放在“触摸栈页”的循环之后：

- 保证编译器不会把循环内的写重排到屏障之后；
- 保证编译器不会因“后续未使用”而优化掉这些写；
- 确保“触摸栈”的语义在单线程、仅考虑编译器优化时得到保留，从而正确触发栈增长与缺页。

若需跨线程可见性或与 DMA/设备的顺序，需使用硬件屏障（如 `atomic_thread_fence` 或显式 MFENCE 等），参见 SDM。

## 与 Intel SDM 的对应关系

- **Volume 3A（System Programming Guide, Part 1）**  
  描述系统架构、多处理器与 **CPU 层面的内存顺序**（如 store buffer、多核可见性、何时需要屏障指令）。  
  本行 asm **不对应** Vol 3A 中的某条指令，它只影响编译器生成的指令顺序；要实现 SDM 描述的内存顺序，需在代码中插入会生成 MFENCE/LFENCE/SFENCE 的屏障或原子 API。

- **Volume 2（Instruction Set Reference, A–Z）**  
  对 **MFENCE**（全屏障）、**LFENCE**（load fence）、**SFENCE**（store fence）等指令有逐条说明。  
  本行 asm **不生成** 上述任何指令，因此从“指令集”角度，Vol 2 中没有与这行 asm 一一对应的条目。

## 参考

- GCC: [Extended Asm - Assembler Instructions with C Expression Operands](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
- Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 3A (内存顺序、多处理器)
- Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2 (MFENCE/LFENCE/SFENCE 指令说明)
