# musl 中 `__stack_chk_guard` 的实现分析

本文基于本地源码目录 `"/Users/weli/works/musl"`，聚焦 `__stack_chk_guard` 在 musl 中的定义、初始化、线程传播和失败处理路径。

---

## 1. 结论先行

- `__stack_chk_guard` 是 **musl 用户态全局变量**，不是内核导出符号。
- 初始化发生在进程启动早期：`__init_libc` -> `__init_ssp((void *)aux[AT_RANDOM])`。
- 随机性主要来自内核通过 auxv 提供的 `AT_RANDOM`。
- 当前线程 canary 会在 `__init_ssp` 中写入 `__pthread_self()->canary`。
- 新线程通过 `pthread_create` 继承父线程 canary。
- canary 校验失败后走 `__stack_chk_fail()`，musl 直接 `a_crash()` 终止。

---

## 2. 核心源码位置

### 2.1 guard 定义与初始化实现

文件：`/Users/weli/works/musl/src/env/__stack_chk_fail.c`

```c
uintptr_t __stack_chk_guard;

void __init_ssp(void *entropy)
{
	if (entropy) memcpy(&__stack_chk_guard, entropy, sizeof(uintptr_t));
	else __stack_chk_guard = (uintptr_t)&__stack_chk_guard * 1103515245;

#if UINTPTR_MAX >= 0xffffffffffffffff
	((char *)&__stack_chk_guard)[1] = 0;
#endif

	__pthread_self()->canary = __stack_chk_guard;
}

void __stack_chk_fail(void)
{
	a_crash();
}
```

#### 解释

1. `entropy` 非空：直接从随机源拷贝 `sizeof(uintptr_t)` 字节。
2. `entropy` 为空：使用地址混合常数生成退化值（兜底，不是强随机）。
3. 64 位下将第 2 字节置 0：减少某些字符串越界场景下泄露/覆盖 canary 的风险。
4. 初始化后同步到当前线程控制块中的 `canary` 字段。

---

### 2.2 初始化调用链（进程启动期）

文件：`/Users/weli/works/musl/src/env/__libc_start_main.c`

```c
void __init_libc(char **envp, char *pn)
{
	size_t i, *auxv, aux[AUX_CNT] = { 0 };
	...
	for (i=0; auxv[i]; i+=2) if (auxv[i]<AUX_CNT) aux[auxv[i]] = auxv[i+1];
	...
	__init_tls(aux);
	__init_ssp((void *)aux[AT_RANDOM]);
	...
}
```

#### 解释

- musl 在启动阶段解析 auxv，取出 `AT_RANDOM`。
- 然后调用 `__init_ssp` 完成栈保护 canary 初始化。
- 所以 `__stack_chk_guard` 在应用主逻辑运行前已经准备好。

---

### 2.3 线程 canary 结构与继承

线程结构中的 canary 字段定义见：

文件：`/Users/weli/works/musl/src/internal/pthread_impl.h`

```c
struct pthread {
	...
	uintptr_t canary;
	...
};
```

新线程继承父线程 canary：

文件：`/Users/weli/works/musl/src/thread/pthread_create.c`

```c
new->canary = self->canary;
```

#### 解释

- 主线程初始化后，后续线程沿用同一 canary 值（按该版本实现）。
- 编译器插桩在函数序言/尾声里使用线程 canary 完成检查。

---

## 3. `__stack_chk_guard` 与内核的边界

### 内核提供什么

- 在 `execve` 时提供 auxv，其中包含 `AT_RANDOM` 指针（指向随机字节）。

### musl 提供什么

- `__stack_chk_guard` 的存储位置与初始化逻辑。
- `__stack_chk_fail()` 的失败处理行为（崩溃终止）。

### 关键认知

- **内核不维护 `__stack_chk_guard` 变量本体**。
- 内核负责提供熵来源；用户态 C 运行时负责 guard 生命周期与校验失败处理。

---

## 4. 失败路径语义

`__stack_chk_fail()` 在该版本 musl 里只有：

```c
void __stack_chk_fail(void)
{
	a_crash();
}
```

这表示：

- 不尝试恢复；
- 不继续执行；
- 以快速崩溃方式结束进程，防止破坏后的控制流继续运行。

---

## 5. 对阅读汇编的帮助

在 aarch64 汇编中若看到如下模式：

1. 读取 `__stack_chk_guard`（通常经 GOT 或线程指针路径）；
2. 在函数入口保存局部 canary；
3. 函数返回前比较；
4. 不等则跳转 `__stack_chk_fail`；

就可将其对应到本文所述的 musl 运行时语义，而不是误认为“内核在做栈保护检查”。

---

## 6. 相关文件清单

- `/Users/weli/works/musl/src/env/__stack_chk_fail.c`
- `/Users/weli/works/musl/src/env/__libc_start_main.c`
- `/Users/weli/works/musl/src/internal/pthread_impl.h`
- `/Users/weli/works/musl/src/thread/pthread_create.c`
- `/Users/weli/works/musl/dynamic.list`

---

## 7. `__stack_chk_guard` 与 `__stack_chk_fail`（语义区分）

| 符号 | ELF 类型 | 热路径做什么 |
|------|-----------|----------------|
| `__stack_chk_guard` | **OBJECT**（数据） | **不**存在「调用 guard」。编译器插桩为 **经 GOT 加载** 全局 canary，与栈槽副本 **比较**。 |
| `__stack_chk_fail` | **FUNC**（函数） | **仅在校验失败** 时 **`bl` → PLT → `__stack_chk_fail`**；正常返回路径 **不会** 执行该调用。 |

因此：源码里看不到对 guard 的「函数调用」，但可执行文件里 **一定有对 guard 数据的引用**（动态链接下多为 **UND + GOT**）。

---

## 8. 动态链接可执行文件中的符号与重定位（实用命令）

在容器或本机对 `heap_vs_stack_fairness`（或任意动态链接 ELF）可执行：

```bash
# 动态符号：未解析符号会显示为 U（UND）
nm -D ./heap_vs_stack_fairness | grep stack_chk

# 符号表（含类型：OBJECT / FUNC）
readelf -Ws ./heap_vs_stack_fairness | grep stack_chk
```

典型输出含义：

- `U __stack_chk_guard`、`U __stack_chk_fail`：主程序 **未定义**，运行时由 **musl** 解析。
- `OBJECT` / `GLOBAL` / `UND`：guard 是 **数据符号**；fail 是 **函数符号**。

**重定位（GOT / PLT）**：

```bash
objdump -R ./heap_vs_stack_fairness | grep stack_chk
```

典型 aarch64 行（地址以实际为准）：

- `R_AARCH64_GLOB_DAT __stack_chk_guard`：GOT 槽最终填 **guard 变量地址**（数据指针）。
- `R_AARCH64_JUMP_SLOT __stack_chk_fail`：PLT 槽解析为 **`__stack_chk_fail` 函数**（失败路径）。

**反汇编里为何 `grep stack_chk_guard` 常为空**：

- `objdump -D` 对 **GOT 访问** 常显示为 `adrp`/`ldr` 与 **GOT 偏移**，**不一定**在每一行打印字符串 `__stack_chk_guard`。
- 用 **`nm -D` / `readelf` / `objdump -R`** 比单纯 **`objdump -D | grep`** 更可靠。

**反汇编里容易看到的是 `__stack_chk_fail@plt`**：

```bash
objdump -D ./heap_vs_stack_fairness | grep __stack_chk
```

若出现多行 `bl ... <__stack_chk_fail@plt>`，表示二进制里有多处 **校验失败出口**（各函数的尾声检查），**不等于**正常运行会频繁进入该路径；**仅 canary 不匹配时** 才会执行这些 `bl`。

**带重定位注释的反汇编**（便于对照 GOT）：

```bash
objdump -d -r ./heap_vs_stack_fairness | less
```

---

## 9. 与项目其它文档的交叉引用

- 容器内 **GCC 汇编**、场景 2 与 **`bl`/`ret`**：**`FAIRNESS_CONTAINER_LINUX_ASM_ANALYSIS.md`**
- 场景 2 与 **CPU 缓存**：**`SCENARIO2_CACHE_NOTE.md`**

---

## 10. 场景2中 guard 机制的“源码 -> 汇编”对应标注

以下以项目里的场景 2 为例，把你看到的 aarch64 指令与源码语义对齐。

### 10.1 业务源码（触发栈保护插桩的函数）

文件：`/Users/weli/works/stack-vs-heap-benchmark/demos/heap_vs_stack_fairness.c`

- `process_with_stack_buffer(int value)` 含 `char buffer[1024]`，会触发编译器 SSP 插桩。
- `test2_stack_with_function_call()` 热循环每次 `bl process_with_stack_buffer`。

### 10.2 汇编中的 guard 关键指令

文件：`/Users/weli/works/stack-vs-heap-benchmark/demos/heap_vs_stack_fairness.linux_container_O2.s.md`

在 `process_with_stack_buffer` 中可见：

- **取 guard 地址（GOT）**：`adrp/ldr ... :got:__stack_chk_guard`
- **读 guard 当前值并保存栈副本**：`ldr x2, [x1]` + `str x2, [sp, 1032]`
- **返回前复读并比较**：`ldr x2, [sp, 1032]` vs `ldr x1, [x0]`，随后 `subs`
- **不一致分支**：`bne .L47`
- **失败处理调用**：`.L47: bl __stack_chk_fail`

这组指令对应的运行时实现在 musl：

- `__stack_chk_guard` 定义与初始化：`/Users/weli/works/musl/src/env/__stack_chk_fail.c`
- `__stack_chk_fail()`：同文件，函数体 `a_crash();`

---

## 11. 如何触发 musl 的 `__stack_chk_fail`（最小可复现）

下面给一个可操作、可验证的方式。核心思路是：**制造栈溢出覆盖 canary**，让函数尾声比较失败，跳到 `__stack_chk_fail`。

### 11.1 示例代码

```c
#include <stdio.h>
#include <string.h>

__attribute__((noinline))
static void smash(const char *input) {
    char local_buffer[16];
    /* 故意越界写，覆盖 canary */
    strcpy(local_buffer, input);
    puts(local_buffer);
}

int main(void) {
    /* 长度远超 16，必然破坏栈布局 */
    smash("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    return 0;
}
```

### 11.2 在容器中编译运行

```bash
cat > /tmp/ssp_crash.c <<'EOF'
#include <stdio.h>
#include <string.h>
__attribute__((noinline))
static void smash(const char *input) {
    char local_buffer[16];
    strcpy(local_buffer, input);
    puts(local_buffer);
}
int main(void) {
    smash("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    return 0;
}
EOF

# 强制启用 SSP，避免工具链默认策略差异
gcc -O2 -fstack-protector-strong /tmp/ssp_crash.c -o /tmp/ssp_crash
/tmp/ssp_crash
echo $?
```

### 11.3 预期现象

- 进程异常终止（常见是 `Aborted` / 非 0 退出码）。
- `objdump -D /tmp/ssp_crash | grep __stack_chk_fail` 通常可见失败分支调用。
- 触发路径是：函数尾声比较失败 -> `bl __stack_chk_fail` -> musl `a_crash()`.

### 11.4 验证 guard/fail 符号与重定位

```bash
nm -D /tmp/ssp_crash | grep stack_chk
readelf -Ws /tmp/ssp_crash | grep stack_chk
objdump -R /tmp/ssp_crash | grep stack_chk
```

你会看到与项目主程序相同的模式：

- `__stack_chk_guard`：`OBJECT` + `UND`
- `__stack_chk_fail`：`FUNC` + `UND`
- `R_AARCH64_GLOB_DAT`（guard）和 `R_AARCH64_JUMP_SLOT`（fail）

