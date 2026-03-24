```asm	
	.arch armv8-a
	.file	"heap_vs_stack_fairness.c"
/* ============================================================================
 * Linux 容器内完整汇编 + 人工标注
 *
 * 生成（仓库根目录）：
 *   docker-compose run --rm benchmark sh -c \
 *     'gcc -S -O2 -fverbose-asm demos/heap_vs_stack_fairness.c \
 *      -o demos/heap_vs_stack_fairness.linux_container_O2.s'
 *
 * 环境：Alpine GCC，aarch64-alpine-linux-musl，-O2，默认启用 -fstack-protector 类选项。
 *
 * 【术语：guard page vs 编译器 SSP】
 * - 内核「栈保护页 / guard page」：栈向低地址增长时，常在映射边界留一页 PROT_NONE（或 guard gap），
 *   栈溢出越过合法映射会 **缺页/SIGSEGV**，属 **内核 MMU + 进程地址空间** 机制；**不**体现为下面
 *   某条固定汇编「读 guard page」。
 * - 编译器 **SSP（Stack Smashing Protector）**：全局变量 `__stack_chk_guard`（金丝雀），经 **GOT**
 *   加载、副本存栈槽、返回前与内存中当前值比较；**全程用户态**，与 musl 初始化（如 AT_RANDOM）的
 *   关系见 MUSL_STACK_CHK_GUARD_ANALYSIS.md；与「内核保护页」是 **不同层次** 的两件事。
 *
 * 正文注释：函数头一行概览；同一句 C 对应多行指令时块首标一次。配套：FAIRNESS_CONTAINER_LINUX_ASM_ANALYSIS.md §5
 * ============================================================================
 */
// GNU C23 (Alpine 15.2.0) version 15.2.0 (aarch64-alpine-linux-musl)
//	compiled by GNU C version 15.2.0, GMP version 6.3.0, MPFR version 4.2.2, MPC version 1.3.1, isl version isl-0.26-GMP

// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed: -march=armv8-a -mlittle-endian -mabi=lp64 -O2
	.text
	.align	2
	.p2align 5,,15
/* 场景1-栈：test1_stack_small_object — 1KB 局部数组 + SSP；热循环 .L2 无 bl */
	.global	test1_stack_small_object
	.type	test1_stack_small_object, %function
test1_stack_small_object:
.LFB0:
	.cfi_startproc
	stp	x29, x30, [sp, -16]!	//,,,
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	/* 【SSP 序言】adrp/ldr 经 GOT 得 guard 变量地址；紧接着 ldr/str 把 *guard 写入栈槽 [sp,#1064]（与内核栈保护页无关）。 */
	adrp	x0, :got:__stack_chk_guard	// tmp122,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp122,
	mov	x29, sp	//,
	sub	sp, sp, #1072	//,,
	.cfi_def_cfa_offset 1088
// test1_stack_small_object() { — 金丝雀副本 [sp,#1064]
	ldr	x1, [x0]	// tmp149,
	str	x1, [sp, 1064]	// tmp149, D.5489
	mov	x1, 0	// tmp149
// clock_gettime(MONOTONIC, &start)
	mov	w0, 1	//,
	add	x1, sp, 8	//,,
	bl	clock_gettime		//
// for (i=0; i<ITERATIONS; i++) — 常量 w5=1_000_000，x3→g_sum
	adrp	x3, .LANCHOR0	// tmp148,
	mov	w5, 16960	// tmp136,
	add	x3, x3, :lo12:.LANCHOR0	// tmp131, tmp148,
	mov	w1, 0	// i,
	movk	w5, 0xf, lsl 16	// tmp136,,
	.p2align 5,,15
.L2:
	// 写 buffer[0]/[1023]；i++；读回；g_sum += …；直到 i==1_000_000
	strb	w1, [sp, 40]	// _128, MEM[(volatile char *)&buffer]
	asr	w0, w1, 8	// _3, i,
	strb	w0, [sp, 1063]	// _3, MEM[(volatile char *)&buffer + 1023B]
	add	w1, w1, 1	// i, i,
	ldrb	w0, [sp, 40]	//, MEM[(volatile char *)&buffer]
	ldrb	w2, [sp, 1063]	//, MEM[(volatile char *)&buffer + 1023B]
	ldr	w4, [x3]	//, g_sum
	and	w2, w2, 255	// _6, MEM[(volatile char *)&buffer + 1023B]
	add	w0, w2, w0, uxtb	// _29, _6, MEM[(volatile char *)&buffer]
	add	w0, w0, w4	// _9, _29, g_sum.0_8
	str	w0, [x3]	// _9, g_sum
	cmp	w1, w5	// i, tmp136
	bne	.L2		//,
// clock_gettime(MONOTONIC, &end)
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// return (end-start) ns
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp142,
	movk	x2, 0x3b9a, lsl 16	// tmp142,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp142, _17
// } — SSP 尾声：比较 [sp,#1064] 与当前 guard
	adrp	x1, :got:__stack_chk_guard	// tmp147,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp147,
	ldr	x3, [sp, 1064]	// tmp150, D.5489
	ldr	x2, [x1]	// tmp151,
	subs	x3, x3, x2	// tmp150, tmp151
	mov	x2, 0	// tmp151
	bne	.L7		//,
	add	sp, sp, 1072	//,,
	.cfi_remember_state
	.cfi_def_cfa_offset 16
	ldp	x29, x30, [sp], 16	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret	
.L7:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE0:
	.size	test1_stack_small_object, .-test1_stack_small_object
	.align	2
	.p2align 5,,15
/* test3_stack_medium_object：256KB 栈对象，100 次 */
	.global	test3_stack_medium_object
	.type	test3_stack_medium_object, %function
test3_stack_medium_object:
.LFB5:
	.cfi_startproc
	stp	x29, x30, [sp, -16]!	//,,,
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	adrp	x0, :got:__stack_chk_guard	// tmp120,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp120,
	mov	x29, sp	//,
	sub	sp, sp, #48	//,,
	.cfi_def_cfa_offset 64
	sub	sp, sp, #262144	//,,
	.cfi_def_cfa_offset 262208
// test3_stack_medium_object() { — 大栈帧；SSP 副本 [sp+262144+40]；clock_gettime(start)；x3→g_sum；for i=0..99
	add	x1, sp, 262144	// tmp149,,
	ldr	x2, [x0]	// tmp144,
	str	x2, [x1, 40]	// tmp144, D.5503
	mov	x2, 0	// tmp144
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	adrp	x3, .LANCHOR0	// tmp143,
	add	x3, x3, :lo12:.LANCHOR0	// tmp127, tmp143,
	mov	w1, 0	// i,
	.p2align 5,,15
.L9:
	// .L9：写 p[0]/末字节、i++、g_sum += p[0]+p[262143]；cmp i,100
	add	x0, sp, 262144	// tmp152,,
	add	x2, sp, 262144	// tmp154,,
	strb	w1, [sp, 40]	// _110, MEM[(volatile char *)&buffer]
	add	w1, w1, 1	// i, i,
	strb	wzr, [x0, 39]	//, MEM[(volatile char *)&buffer + 262143B]
	ldrb	w0, [sp, 40]	//, MEM[(volatile char *)&buffer]
	ldrb	w2, [x2, 39]	//, MEM[(volatile char *)&buffer + 262143B]
	ldr	w4, [x3]	//, g_sum
	and	w2, w2, 255	// _4, MEM[(volatile char *)&buffer + 262143B]
	add	w0, w2, w0, uxtb	// _27, _4, MEM[(volatile char *)&buffer]
	add	w0, w0, w4	// _7, _27, g_sum.4_6
	str	w0, [x3]	// _7, g_sum
	cmp	w1, 100	// i,
	bne	.L9		//,
// clock_gettime(end)；return ns；add x3 指向大帧顶以便读 SSP 副本
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	add	x3, sp, 262144	// tmp156,,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _10, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _15, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp137,
	movk	x2, 0x3b9a, lsl 16	// tmp137,,
	madd	x0, x0, x2, x1	// <retval>, _10, tmp137, _15
// } — SSP 尾声
	adrp	x1, :got:__stack_chk_guard	// tmp142,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp142,
	ldr	x4, [x3, 40]	// tmp145, D.5503
	ldr	x2, [x1]	// tmp146,
	subs	x4, x4, x2	// tmp145, tmp146
	mov	x2, 0	// tmp146
	bne	.L13		//,
	add	sp, sp, 48	//,,
	.cfi_remember_state
	.cfi_def_cfa_offset 262160
	add	sp, sp, 262144	//,,
	.cfi_def_cfa_offset 16
	ldp	x29, x30, [sp], 16	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret	
.L13:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE5:
	.size	test3_stack_medium_object, .-test3_stack_medium_object
	.align	2
	.p2align 5,,15
/* test1_heap_small_object：每轮 malloc+free */
	.global	test1_heap_small_object
	.type	test1_heap_small_object, %function
test1_heap_small_object:
.LFB1:
	.cfi_startproc
	sub	sp, sp, #96	//,,
	.cfi_def_cfa_offset 96
	adrp	x0, :got:__stack_chk_guard	// tmp123,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp123,
	stp	x29, x30, [sp, 48]	//,,
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	add	x29, sp, 48	//,,
	stp	x19, x20, [sp, 64]	//,,
	.cfi_offset 19, -32
	.cfi_offset 20, -24
	adrp	x20, .LANCHOR0	// tmp149,
	add	x20, x20, :lo12:.LANCHOR0	// tmp132, tmp149,
// test1_heap_small_object() { — SSP [sp,#40]；x20→g_sum；w21=ITERATIONS；clock_gettime(start)
	str	x21, [sp, 80]	//,
	.cfi_offset 21, -16
	mov	w21, 16960	// tmp137,
	mov	w19, 0	// i,
	movk	w21, 0xf, lsl 16	// tmp137,,
	ldr	x1, [x0]	// tmp152,
	str	x1, [sp, 40]	// tmp152, D.5517
	mov	x1, 0	// tmp152
	mov	w0, 1	//,
	add	x1, sp, 8	//,,
	bl	clock_gettime		//
	.p2align 5,,15
.L15:
	// .L15：malloc(1KB)；写两端；i++；g_sum += …；free；直到 i==1_000_000
	mov	x0, 1024	//,
	bl	malloc		//
	strb	w19, [x0]	// _130, MEM[(volatile char *)buffer_28]
	asr	w2, w19, 8	// _3, i,
	strb	w2, [x0, 1023]	// _3, MEM[(volatile char *)buffer_28 + 1023B]
	add	w19, w19, 1	// i, i,
	ldrb	w2, [x0]	//, MEM[(volatile char *)buffer_28]
	ldrb	w1, [x0, 1023]	//, MEM[(volatile char *)buffer_28 + 1023B]
	ldr	w3, [x20]	//, g_sum
	and	w1, w1, 255	// _6, MEM[(volatile char *)buffer_28 + 1023B]
	add	w1, w1, w2, uxtb	// _31, _6, MEM[(volatile char *)buffer_28]
	add	w1, w1, w3	// _9, _31, g_sum.1_8
	str	w1, [x20]	// _9, g_sum
	bl	free		//
	cmp	w19, w21	// i, tmp137
	bne	.L15		//,
// clock_gettime(end)；return ns
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp143,
	movk	x2, 0x3b9a, lsl 16	// tmp143,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp143, _17
// } — SSP 尾声
	adrp	x1, :got:__stack_chk_guard	// tmp148,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp148,
	ldr	x3, [sp, 40]	// tmp153, D.5517
	ldr	x2, [x1]	// tmp154,
	subs	x3, x3, x2	// tmp153, tmp154
	mov	x2, 0	// tmp154
	bne	.L19		//,
	ldr	x21, [sp, 80]	//,
	ldp	x29, x30, [sp, 48]	//,,
	ldp	x19, x20, [sp, 64]	//,,
	add	sp, sp, 96	//,,
	.cfi_remember_state
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret	
.L19:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE1:
	.size	test1_heap_small_object, .-test1_heap_small_object
	.align	2
	.p2align 5,,15
/* 场景2-堆复用：test2_heap_reuse — malloc 与 clock_gettime(start) 在 .L21 前；计时段仅 .L21；free 在计时后；x19=buffer */
	.global	test2_heap_reuse
	.type	test2_heap_reuse, %function
test2_heap_reuse:
.LFB4:
	.cfi_startproc
	sub	sp, sp, #80	//,,
	.cfi_def_cfa_offset 80
	adrp	x0, :got:__stack_chk_guard	// tmp123,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp123,
	stp	x29, x30, [sp, 48]	//,,
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	add	x29, sp, 48	//,,
	str	x19, [sp, 64]	//,
	.cfi_offset 19, -16
// test2_heap_reuse() { — SSP 副本 [sp,#40]；buffer = malloc(1024)；clock_gettime(start)
	ldr	x1, [x0]	// tmp151,
	str	x1, [sp, 40]	// tmp151, D.5531
	mov	x1, 0	// tmp151
	mov	x0, 1024	//,
	bl	malloc		//
	mov	x19, x0	// buffer, buffer
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// for (i=0; i<ITERATIONS; i++) — w5=1_000_000，x3→g_sum，w2=i
	adrp	x3, .LANCHOR0	// tmp149,
	mov	w5, 16960	// tmp137,
	add	x3, x3, :lo12:.LANCHOR0	// tmp132, tmp149,
	mov	w2, 0	// i,
	movk	w5, 0xf, lsl 16	// tmp137,,
	.p2align 5,,15
.L21:
	// 与 test1 热循环同语义；buffer 基址在 x19（堆），无每轮 bl
	strb	w2, [x19]	// _130, MEM[(volatile char *)buffer_23]
	asr	w0, w2, 8	// _3, i,
	strb	w0, [x19, 1023]	// _3, MEM[(volatile char *)buffer_23 + 1023B]
	add	w2, w2, 1	// i, i,
	ldrb	w1, [x19]	//, MEM[(volatile char *)buffer_23]
	ldrb	w0, [x19, 1023]	//, MEM[(volatile char *)buffer_23 + 1023B]
	ldr	w4, [x3]	//, g_sum
	and	w0, w0, 255	// _6, MEM[(volatile char *)buffer_23 + 1023B]
	add	w1, w0, w1, uxtb	// _32, _6, MEM[(volatile char *)buffer_23]
	add	w1, w1, w4	// _9, _32, g_sum.3_8
	str	w1, [x3]	// _9, g_sum
	cmp	w2, w5	// i, tmp137
	bne	.L21		//,
// clock_gettime(end)；free(buffer)；return ns
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	mov	x0, x19	//, buffer
	bl	free		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp143,
	movk	x2, 0x3b9a, lsl 16	// tmp143,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp143, _17
// } — SSP 尾声
	adrp	x1, :got:__stack_chk_guard	// tmp148,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp148,
	ldr	x3, [sp, 40]	// tmp152, D.5531
	ldr	x2, [x1]	// tmp153,
	subs	x3, x3, x2	// tmp152, tmp153
	mov	x2, 0	// tmp153
	bne	.L25		//,
	ldr	x19, [sp, 64]	//,
	ldp	x29, x30, [sp, 48]	//,,
	add	sp, sp, 80	//,,
	.cfi_remember_state
	.cfi_restore 19
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret	
.L25:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE4:
	.size	test2_heap_reuse, .-test2_heap_reuse
	.align	2
	.p2align 5,,15
/* test3_heap_large_object：每轮 1MB malloc+free */
	.global	test3_heap_large_object
	.type	test3_heap_large_object, %function
test3_heap_large_object:
.LFB6:
	.cfi_startproc
	sub	sp, sp, #80	//,,
	.cfi_def_cfa_offset 80
	adrp	x0, :got:__stack_chk_guard	// tmp121,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp121,
	stp	x29, x30, [sp, 48]	//,,
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	add	x29, sp, 48	//,,
	stp	x19, x20, [sp, 64]	//,,
	.cfi_offset 19, -16
	.cfi_offset 20, -8
// test3_heap_large_object() { — SSP [sp,#40]；x20→g_sum；clock_gettime(start)；for i=0..99
	adrp	x20, .LANCHOR0	// tmp147,
	add	x20, x20, :lo12:.LANCHOR0	// tmp148, tmp147,
	mov	w19, 0	// i,
	ldr	x1, [x0]	// tmp150,
	str	x1, [sp, 40]	// tmp150, D.5545
	mov	x1, 0	// tmp150
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	.p2align 5,,15
.L28:
	// .L28：malloc(1MB)；若非空则写首尾、g_sum += …、free；.L27：仅 i++
	mov	x0, 1048576	//,
	bl	malloc		//
	cbz	x0, .L27	// tmp123,
	add	x2, x0, 1044480	// tmp125, tmp123,
	strb	w19, [x0]	// _113, MEM[(volatile char *)buffer_27]
	strb	wzr, [x2, 4095]	//, MEM[(volatile char *)buffer_27 + 1048575B]
	ldrb	w1, [x0]	//, MEM[(volatile char *)buffer_27]
	ldrb	w2, [x2, 4095]	//, MEM[(volatile char *)buffer_27 + 1048575B]
	ldr	w3, [x20]	//, g_sum
	and	w2, w2, 255	// _4, MEM[(volatile char *)buffer_27 + 1048575B]
	add	w1, w2, w1, uxtb	// _30, _4, MEM[(volatile char *)buffer_27]
	add	w1, w1, w3	// _7, _30, g_sum.5_6
	str	w1, [x20]	// _7, g_sum
	bl	free		//
.L27:
	add	w19, w19, 1	// i, i,
	cmp	w19, 100	// i,
	bne	.L28		//,
// clock_gettime(end)；return ns
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _10, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _15, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp140,
	movk	x2, 0x3b9a, lsl 16	// tmp140,,
	madd	x0, x0, x2, x1	// <retval>, _10, tmp140, _15
// } — SSP 尾声
	adrp	x1, :got:__stack_chk_guard	// tmp145,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp145,
	ldr	x3, [sp, 40]	// tmp151, D.5545
	ldr	x2, [x1]	// tmp152,
	subs	x3, x3, x2	// tmp151, tmp152
	mov	x2, 0	// tmp152
	bne	.L35		//,
	ldp	x29, x30, [sp, 48]	//,,
	ldp	x19, x20, [sp, 64]	//,,
	add	sp, sp, 80	//,,
	.cfi_remember_state
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret	
.L35:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE6:
	.size	test3_heap_large_object, .-test3_heap_large_object
	.align	2
	.p2align 5,,15
/* 场景4：test4_heap_with_pool — 内联 pool 初始化；计时段含 .L38（每轮 pool_alloc 内联 + 与场景1 同构热循环） */
	.global	test4_heap_with_pool
	.type	test4_heap_with_pool, %function
test4_heap_with_pool:
.LFB10:
	.cfi_startproc
	sub	sp, sp, #80	//,,
	.cfi_def_cfa_offset 80
	adrp	x0, :got:__stack_chk_guard	// tmp132,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp132,
	stp	x29, x30, [sp, 48]	//,,
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	add	x29, sp, 48	//,,
	str	x19, [sp, 64]	//,
	.cfi_offset 19, -16
// test4_heap_with_pool() { — SSP 副本 [sp,#40]；内联构造 pool（malloc×2、填字段）；clock_gettime(start)
	ldr	x1, [x0]	// tmp171,
	str	x1, [sp, 40]	// tmp171, D.5569
	mov	x1, 0	// tmp171
	mov	x0, 32	//,
	bl	malloc		//
	mov	x19, x0	// pool, pool
	mov	x0, 40960	//,
	movk	x0, 0xf, lsl 16	//,,
	bl	malloc		//
	str	x0, [x19]	// tmp170, pool_46->memory
	adrp	x0, .LC0	// tmp176,
	str	xzr, [x19, 24]	//, pool_46->next_free
	add	x1, sp, 8	//,,
	ldr	q31, [x0, #:lo12:.LC0]	// tmp136,
	mov	w0, 1	//,
	str	q31, [x19, 8]	// tmp136, MEM <vector(2) long unsigned int> [(long unsigned int *)pool_46 + 8B]
	bl	clock_gettime		//
// for (i=0; i<ITERATIONS; i++) — w6=1_000_000，x4→g_sum，w0=i
	adrp	x4, .LANCHOR0	// tmp166,
	mov	w6, 16960	// tmp153,
	add	x4, x4, :lo12:.LANCHOR0	// tmp148, tmp166,
	mov	w0, 0	// i,
	movk	w6, 0xf, lsl 16	// tmp153,,
	.p2align 5,,15
.L38:
	// 每轮：pool_alloc 内联（next_free 回绕、算 ptr）→ 写 p[0]/p[1023] → i++ → g_sum → 循环
	ldp	x2, x1, [x19, 16]	// pool_46->total_blocks, _42,
	add	x5, x1, 1	// _63, _42,
	cmp	x1, x2	// _42, pool_46->total_blocks
	bcc	.L37		//,
	mov	x5, 1	// _63,
	mov	x1, 0	// _42,
.L37:
	ldp	x2, x3, [x19]	// pool_46->memory, pool_46->block_size,* pool
	str	x5, [x19, 24]	// _63, pool_46->next_free
	asr	w5, w0, 8	// _74, i,
	mul	x1, x1, x3	// _68, _42, pool_46->block_size
	add	x3, x2, x1	// ptr, pool_46->memory, _68
	strb	w0, [x2, x1]	// _7, MEM[(volatile char *)ptr_69]
	add	w0, w0, 1	// i, i,
	strb	w5, [x3, 1023]	// _74, MEM[(volatile char *)ptr_69 + 1023B]
	ldrb	w1, [x2, x1]	//, MEM[(volatile char *)ptr_69]
	ldrb	w2, [x3, 1023]	//, MEM[(volatile char *)ptr_69 + 1023B]
	ldr	w3, [x4]	//, g_sum
	and	w2, w2, 255	// _78, MEM[(volatile char *)ptr_69 + 1023B]
	add	w1, w2, w1, uxtb	// _80, _78, MEM[(volatile char *)ptr_69]
	add	w1, w1, w3	// _82, _80, g_sum.6_81
	str	w1, [x4]	// _82, g_sum
	cmp	w0, w6	// i, tmp153
	bne	.L38		//,
// clock_gettime(end)；pool_destroy 等价（free memory + free pool）；return ns
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldr	x0, [x19]	//, pool_46->memory
	bl	free		//
	mov	x0, x19	//, pool
	bl	free		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp160,
	movk	x2, 0x3b9a, lsl 16	// tmp160,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp160, _17
// } — SSP 尾声
	adrp	x1, :got:__stack_chk_guard	// tmp165,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp165,
	ldr	x3, [sp, 40]	// tmp172, D.5569
	ldr	x2, [x1]	// tmp173,
	subs	x3, x3, x2	// tmp172, tmp173
	mov	x2, 0	// tmp173
	bne	.L43		//,
	ldr	x19, [sp, 64]	//,
	ldp	x29, x30, [sp, 48]	//,,
	add	sp, sp, 80	//,,
	.cfi_remember_state
	.cfi_restore 19
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret	
.L43:
	.cfi_restore_state
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE10:
	.size	test4_heap_with_pool, .-test4_heap_with_pool
	.align	2
	.p2align 5,,15
/* 【场景2-栈-子函数】process_with_stack_buffer：
 * 【栈帧】sub sp,sp,#1040 — char buffer[1024] + 金丝雀槽（与内核「guard page」无关，见文件头）。
 * 【SSP】每调一次：序言经 GOT 取 __stack_chk_guard，副本存 [sp,#1032]；尾声 subs 与当前 *guard 比较，失败 bl __stack_chk_fail。
 */
	.global	process_with_stack_buffer
	.type	process_with_stack_buffer, %function
process_with_stack_buffer:
.LFB2:
	.cfi_startproc
	stp	x29, x30, [sp, -16]!	//,,,
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
/* 【guard-步骤1】adrp/ldr：经 GOT 得到全局 __stack_chk_guard 的地址（数据符号，不是调用 guard）。 */
	adrp	x1, :got:__stack_chk_guard	// tmp112,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp112,
	mov	x29, sp	//,
	sub	sp, sp, #1040	//,,
	.cfi_def_cfa_offset 1056
// void process_with_stack_buffer(int value) {
/* 【guard-步骤2】ldr x2,[x1]：读当前 canary；str 写入本栈帧槽 [sp,#1032]（返回前要比对的副本）。 */
	ldr	x2, [x1]	// tmp127,
	str	x2, [sp, 1032]	// tmp127, D.5573
	mov	x2, 0	// tmp127
	adrp	x2, .LANCHOR0	// tmp121,
	strb	w0, [sp, 8]	// _1, MEM[(volatile char *)&buffer]
	asr	w0, w0, 8	// _3, value,
	strb	w0, [sp, 1031]	// _3, MEM[(volatile char *)&buffer + 1023B]
	ldrb	w0, [sp, 8]	//, MEM[(volatile char *)&buffer]
	ldrb	w1, [sp, 1031]	//, MEM[(volatile char *)&buffer + 1023B]
	ldr	w3, [x2, #:lo12:.LANCHOR0]	//, g_sum
	and	w1, w1, 255	// _6, MEM[(volatile char *)&buffer + 1023B]
	add	w0, w1, w0, uxtb	// _14, _6, MEM[(volatile char *)&buffer]
	add	w0, w0, w3	// _9, _14, g_sum.2_8
	str	w0, [x2, #:lo12:.LANCHOR0]	// _9, g_sum
// }
/* 【guard-步骤3】再次经 GOT 取 guard，ldr x1,[x0] 得 *guard；与 [sp,#1032] 副本 subs 比较。 */
	adrp	x0, :got:__stack_chk_guard	// tmp125,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp125,
	ldr	x2, [sp, 1032]	// tmp128, D.5573
	ldr	x1, [x0]	// tmp129,
	subs	x2, x2, x1	// tmp128, tmp129
	mov	x1, 0	// tmp129
/* 【guard-步骤4】不相等则金丝雀被破坏 → bne 到失败路径（栈溢出或内存破坏等）。 */
	bne	.L47		//,
	add	sp, sp, 1040	//,,
	.cfi_remember_state
	.cfi_def_cfa_offset 16
	ldp	x29, x30, [sp], 16	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret	
.L47:
	.cfi_restore_state
/* 【guard-失败】bl __stack_chk_fail@plt（musl 内通常 abort/a_crash）。 */
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE2:
	.size	process_with_stack_buffer, .-process_with_stack_buffer
	.align	2
	.p2align 5,,15
/* 【场景2-栈-外层】test2_stack_with_function_call：
 * 【热循环 .L49】每轮 bl process_with_stack_buffer（子函数内另有完整 SSP 序列，见上）。
 * 本函数另有【函数级】金丝雀副本 [sp,#40]，与百万次子函数调用独立。
 */
	.global	test2_stack_with_function_call
	.type	test2_stack_with_function_call, %function
test2_stack_with_function_call:
.LFB3:
	.cfi_startproc
	sub	sp, sp, #80	//,,
	.cfi_def_cfa_offset 80
	adrp	x0, :got:__stack_chk_guard	// tmp112,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp112,
	stp	x29, x30, [sp, 48]	//,,
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	add	x29, sp, 48	//,,
	stp	x19, x20, [sp, 64]	//,,
	.cfi_offset 19, -16
	.cfi_offset 20, -8
	mov	w20, 16960	// tmp114,
	mov	w19, 0	// i,
	movk	w20, 0xf, lsl 16	// tmp114,,
/* 【函数级 guard-入】外层保存 canary 副本到 [sp,#40]（与 process_with_stack_buffer 的 #1032 不同层栈帧）。 */
	ldr	x1, [x0]	// tmp126,
	str	x1, [sp, 40]	// tmp126, D.5584
	mov	x1, 0	// tmp126
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	.p2align 5,,15
/* 【热循环 .L49】百万次 bl/ret + 子函数内 SSP；计时段含全部 bl。 */
.L49:
	mov	w0, w19	//, i
	add	w19, w19, 1	// i, i,
	bl	process_with_stack_buffer		//
	cmp	w19, w20	// i, tmp114
	bne	.L49		//,
// clock_gettime(end)；return ns
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _3, end.tv_sec, start.tv_sec
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _8, end.tv_nsec, start.tv_nsec
	mov	x2, 51712	// tmp120,
	movk	x2, 0x3b9a, lsl 16	// tmp120,,
	madd	x0, x0, x2, x1	// <retval>, _3, tmp120, _8
// }
/* 【函数级 guard-出】比较 [sp,#40] 与当前 *guard；失败 bne .L53。 */
	adrp	x1, :got:__stack_chk_guard	// tmp125,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp125,
	ldr	x3, [sp, 40]	// tmp127, D.5584
	ldr	x2, [x1]	// tmp128,
	subs	x3, x3, x2	// tmp127, tmp128
	mov	x2, 0	// tmp128
	bne	.L53		//,
	ldp	x29, x30, [sp, 48]	//,,
	ldp	x19, x20, [sp, 64]	//,,
	add	sp, sp, 80	//,,
	.cfi_remember_state
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret	
.L53:
	.cfi_restore_state
/* 【函数级 guard-失败】同 __stack_chk_fail。 */
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE3:
	.size	test2_stack_with_function_call, .-test2_stack_with_function_call
	.align	2
	.p2align 5,,15
/* 场景4：pool_create / pool_alloc / pool_destroy */
	.global	pool_create
	.type	pool_create, %function
pool_create:
.LFB7:
	.cfi_startproc
	stp	x29, x30, [sp, -48]!	//,,,
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -32
	.cfi_offset 20, -24
	mov	x20, x1	// num_blocks, num_blocks
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -16
// pool_create：两次 malloc（struct + 池内存），填字段
	mov	x21, x0	// block_size, block_size
	mov	x0, 32	//,
	bl	malloc		//
	mov	x19, x0	// tmp106, tmp112
	mul	x0, x21, x20	//, block_size, num_blocks
	bl	malloc		//
	stp	x0, x21, [x19]	// tmp113, block_size,
	mov	x0, x19	//, tmp106
	stp	x20, xzr, [x19, 16]	// num_blocks,,
	ldr	x21, [sp, 32]	//,
	ldp	x19, x20, [sp, 16]	//,,
	ldp	x29, x30, [sp], 48	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.cfi_endproc
.LFE7:
	.size	pool_create, .-pool_create
	.align	2
	.p2align 5,,15
/* pool_alloc：next_free 回绕 + madd 算块指针 */
	.global	pool_alloc
	.type	pool_alloc, %function
pool_alloc:
.LFB8:
	.cfi_startproc
// pool_alloc 主体
	ldp	x2, x1, [x0, 16]	// pool_10(D)->total_blocks, _1,
	add	x4, x1, 1	// _18, _1,
	cmp	x1, x2	// _1, pool_10(D)->total_blocks
	bcc	.L57		//,
	mov	x4, 1	// _18,
	mov	x1, 0	// _1,
.L57:
	ldp	x2, x3, [x0]	// pool_10(D)->memory, pool_10(D)->block_size,* pool
	str	x4, [x0, 24]	// _18, pool_10(D)->next_free
	madd	x0, x1, x3, x2	//, _1, pool_10(D)->block_size, pool_10(D)->memory
	ret	
	.cfi_endproc
.LFE8:
	.size	pool_alloc, .-pool_alloc
	.align	2
	.p2align 5,,15
/* pool_destroy：free(pool->memory)；tail call free(pool) */
	.global	pool_destroy
	.type	pool_destroy, %function
pool_destroy:
.LFB9:
	.cfi_startproc
	stp	x29, x30, [sp, -32]!	//,,,
	.cfi_def_cfa_offset 32
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	mov	x29, sp	//,
	str	x19, [sp, 16]	//,
	.cfi_offset 19, -16
	mov	x19, x0	// pool, pool
	ldr	x0, [x0]	//, pool_3(D)->memory
	bl	free		//
	mov	x0, x19	//, pool
	ldr	x19, [sp, 16]	//,
	ldp	x29, x30, [sp], 32	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_def_cfa_offset 0
	b	free		//
	.cfi_endproc
.LFE9:
	.size	pool_destroy, .-pool_destroy
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC1:
	.base64	"ICAlLTQwczogJTguM2YgbXMgKOW5s+WdhyAlNi4xZiBucy/mrKEpCgA="
	.text
	.align	2
	.p2align 5,,15
/* print_result：ns/次 与 ms 的浮点除法 → printf */
	.global	print_result
	.type	print_result, %function
print_result:
.LFB11:
	.cfi_startproc
// printf(耗时格式串)
	ucvtf	d0, x1	// _1, time_ns
	scvtf	d1, w2	// _3, iterations
	mov	x1, 145685290680320	// tmp112,
	movk	x1, 0x412e, lsl 48	// tmp112,,
	fmov	d31, x1	// tmp119, tmp112
	mov	x1, x0	//, name
	adrp	x0, .LC1	// tmp114,
	fdiv	d1, d0, d1	//, _1, _3
	add	x0, x0, :lo12:.LC1	//, tmp114,
	fdiv	d0, d0, d31	//, _1, tmp119
	b	printf		//
	.cfi_endproc
.LFE11:
	.size	print_result, .-print_result
	.section	.rodata.str1.8
	.align	3
.LC2:
	.string	"\n%s\n"
	.align	3
.LC3:
	.string	"========================================"
	.align	3
.LC4:
	.base64	"5aCGL+agiOavlOS+iwA="
	.align	3
.LC5:
	.string	"  %-40s: %.2fx\n"
	.align	3
.LC6:
	.base64	"ICDinJMg6L+Z5Liq5Zy65pmv5aCG5pu05b+r77yI5oiW5o6l6L+R77yJAA=="
	.align	3
.LC7:
	.base64	"ICDimqDvuI8g5aCG5Y+q5oWiICUuMGYlJe+8jOWPr+aOpeWPlwoA"
	.align	3
.LC8:
	.base64	"ICDinJcg5qCI5pi+6JGX5pu05b+r77yI5aCG5oWiICUuMWZ477yJCgA="
	.text
	.align	2
	.p2align 5,,15
/* run_test：blr 调栈/堆测试函数，打印比例 */
	.global	run_test
	.type	run_test, %function
run_test:
.LFB12:
	.cfi_startproc
	stp	x29, x30, [sp, -96]!	//,,,
	.cfi_def_cfa_offset 96
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	mov	x29, sp	//,
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -64
	mov	x21, x1	// stack_name, stack_name
// run_test(...) { — 标题行；存 stack_test/heap_test 函数指针；w19=iterations
	mov	x1, x0	//, title
	adrp	x0, .LC2	// tmp120,
	add	x0, x0, :lo12:.LC2	//, tmp120,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	w19, w5	// iterations, iterations
	mov	x20, x3	// heap_name, heap_name
	stp	d12, d13, [sp, 48]	//,,
	stp	d14, d15, [sp, 64]	//,,
	.cfi_offset 76, -48
	.cfi_offset 77, -40
	.cfi_offset 78, -32
	.cfi_offset 79, -24
	stp	x4, x2, [sp, 80]	// heap_test, stack_test,
	bl	printf		//
	adrp	x0, .LC3	// tmp122,
	add	x0, x0, :lo12:.LC3	//, tmp122,
	bl	puts		//
// stack_time = stack_test()
	ldr	x2, [sp, 88]	// stack_test, %sfp
	blr	x2		// stack_test
// print_result(stack_name, stack_time)
	ucvtf	d14, x0	// _30, stack_time
	scvtf	d12, w19	// _32, iterations
	mov	x0, 145685290680320	// tmp126,
	movk	x0, 0x412e, lsl 48	// tmp126,,
	fmov	d13, x0	// tmp125, tmp126
	mov	x1, x21	//, stack_name
	adrp	x19, .LC1	// tmp128,
	fdiv	d0, d14, d13	//, _30, tmp125
	add	x0, x19, :lo12:.LC1	//, tmp128,
	fdiv	d1, d14, d12	//, _30, _32
	bl	printf		//
// heap_time = heap_test()；再 print_result(heap_name, heap_time)
	ldr	x4, [sp, 80]	// heap_test, %sfp
	blr	x4		// heap_test
	ucvtf	d15, x0	// _26, heap_time
	mov	x1, x20	//, heap_name
	add	x0, x19, :lo12:.LC1	//, tmp128,
	fdiv	d0, d15, d13	//, _26, tmp125
	fdiv	d1, d15, d12	//, _26, _32
	bl	printf		//
// ratio = heap/stack；printf 比例；fcmpe ratio 与 1.0、2.0 → .L67 / .L68 / 默认；各支尾 tail-call printf/puts，与恢复 callee-saved 交错
	fdiv	d15, d15, d14	// ratio, _26, _30
	adrp	x1, .LC4	// tmp136,
	adrp	x0, .LC5	// tmp138,
	add	x1, x1, :lo12:.LC4	//, tmp136,
	add	x0, x0, :lo12:.LC5	//, tmp138,
	fmov	d0, d15	//, ratio
	bl	printf		//
	fmov	d31, 1.0e+0	// tmp139,
	fcmpe	d15, d31	// ratio, tmp139
	bmi	.L67		//,
	fmov	d30, 2.0e+0	// tmp142,
	fcmpe	d15, d30	// ratio, tmp142
	bmi	.L68		//,
	// ratio≥2：printf LC8「栈显著更快…」
	ldr	x21, [sp, 32]	//,
	fmov	d0, d15	//, ratio
	ldp	x19, x20, [sp, 16]	//,,
	adrp	x0, .LC8	// tmp151,
	ldp	d12, d13, [sp, 48]	//,,
	add	x0, x0, :lo12:.LC8	//, tmp151,
	ldp	d14, d15, [sp, 64]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 78
	.cfi_restore 79
	.cfi_restore 76
	.cfi_restore 77
	.cfi_def_cfa_offset 0
	b	printf		//
	.p2align 2,,3
.L68:
	.cfi_restore_state
	// 1≤ratio<2：printf LC7「堆只慢…%」
	fsub	d0, d15, d31	// _3, ratio, tmp139
	mov	x0, 4636737291354636288	// tmp147,
	ldr	x21, [sp, 32]	//,
	fmov	d31, x0	// tmp161, tmp147
	ldp	x19, x20, [sp, 16]	//,,
	fmul	d0, d0, d31	//, _3, tmp161
	ldp	d12, d13, [sp, 48]	//,,
	adrp	x0, .LC7	// tmp149,
	ldp	d14, d15, [sp, 64]	//,,
	add	x0, x0, :lo12:.LC7	//, tmp149,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 78
	.cfi_restore 79
	.cfi_restore 76
	.cfi_restore 77
	.cfi_def_cfa_offset 0
	b	printf		//
	.p2align 2,,3
.L67:
	.cfi_restore_state
	// ratio<1：puts LC6「堆更快…」
	ldr	x21, [sp, 32]	//,
	adrp	x0, .LC6	// tmp141,
	ldp	x19, x20, [sp, 16]	//,,
	add	x0, x0, :lo12:.LC6	//, tmp141,
	ldp	d12, d13, [sp, 48]	//,,
	ldp	d14, d15, [sp, 64]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 78
	.cfi_restore 79
	.cfi_restore 76
	.cfi_restore 77
	.cfi_def_cfa_offset 0
	b	puts		//
	.cfi_endproc
.LFE12:
	.size	run_test, .-run_test
	.section	.rodata.str1.8
	.align	3
.LC9:
	.string	"======================================"
	.align	3
.LC10:
	.base64	"5YWs5bmz55qE5aCGIHZzIOagiOaAp+iDveWvueavlOa1i+ivlQA="
	.align	3
.LC11:
	.base64	"Cuebruagh++8muWxleekuuWQhOiHqumAguWQiOeahOWcuuaZr++8jOiAjOmdnueugOWNleeahCLosIHlv6vnlKjosIEiAA=="
	.align	3
.LC12:
	.base64	"5aCG77yIbWFsbG9jL2ZyZWXvvIkA"
	.align	3
.LC13:
	.base64	"5qCI77yI5bGA6YOo5Y+Y6YeP77yJAA=="
	.align	3
.LC14:
	.base64	"5Zy65pmvMe+8muWwj+Wvueixoe+8iDFLQu+8ieefreeUn+WRveWRqOacnyAtIDEwMOS4h+asoQA="
	.align	3
.LC15:
	.base64	"5aCG77yI5YiG6YWN5LiA5qyh77yM6YeN5aSN5L2/55So77yJAA=="
	.align	3
.LC16:
	.base64	"5qCI77yI5q+P5qyh5Ye95pWw6LCD55So77yJAA=="
	.align	3
.LC17:
	.base64	"5Zy65pmvMu+8muWvueixoeWkjeeUqCAtIDEwMOS4h+asoeS9v+eUqAA="
	.align	3
.LC18:
	.base64	"5aCG77yIMU1C77yM5peg5Y6L5Yqb77yJAA=="
	.align	3
.LC19:
	.base64	"5qCI77yIMjU2S0LvvIzlnKjlronlhajojIPlm7TvvIkA"
	.align	3
.LC20:
	.base64	"5Zy65pmvM++8muWkp+WvueixoeWIhumFjSAtIDEwMOasoQA="
	.align	3
.LC21:
	.base64	"5aCG77yI5YaF5a2Y5rGg77yMTygxKeWIhumFje+8iQA="
	.align	3
.LC22:
	.base64	"5Zy65pmvNO+8muWGheWtmOaxoOS8mOWMliAtIDEwMOS4h+asoQA="
	.align	3
.LC23:
	.string	"\n======================================"
	.align	3
.LC24:
	.base64	"57uT6K66AA=="
	.align	3
.LC25:
	.base64	"4pyTIOWcuuaZrzHvvJrmoIjlrozog5zvvIjlsI/lr7nosaHnn63nlJ/lkb3lkajmnJ/vvIkA"
	.align	3
.LC26:
	.base64	"ICDihpIg6L+Z5piv5qCI55qE5LyY5Yq/5Zy65pmvCgA="
	.align	3
.LC27:
	.base64	"4pqW77iPIOWcuuaZrzLvvJrloIblj6/og73mm7Tlpb3vvIjlr7nosaHlpI3nlKjvvIkA"
	.align	3
.LC28:
	.base64	"ICDihpIg5aCG5YiG6YWN5LiA5qyhIHZzIOagiOavj+asoeWHveaVsOiwg+eUqAA="
	.align	3
.LC29:
	.base64	"ICDihpIg5Y+W5Yaz5LqO5Ye95pWw6LCD55So5byA6ZSACgA="
	.align	3
.LC30:
	.base64	"4pyTIOWcuuaZrzPvvJrloIblv4XpnIDvvIjlpKflr7nosaHvvIkA"
	.align	3
.LC31:
	.base64	"ICDihpIg5qCI5pyJ5aSn5bCP6ZmQ5Yi277yIfjhNQu+8iQA="
	.align	3
.LC32:
	.base64	"ICDihpIg5aCG5Y+v5Lul5YiG6YWN5Lu75oSP5aSn5bCPCgA="
	.align	3
.LC33:
	.base64	"4pqW77iPIOWcuuaZrzTvvJrloIbmjqXov5HmoIjvvIjlhoXlrZjmsaDvvIkA"
	.align	3
.LC34:
	.base64	"ICDihpIg5YaF5a2Y5rGg5LyY5YyW5ZCO77yM5aCG5Y+v5Lul5b6I5b+rAA=="
	.align	3
.LC35:
	.base64	"ICDihpIgdGNtYWxsb2MvamVtYWxsb2Pmm7Tlv6sKAA=="
	.align	3
.LC36:
	.base64	"5qC45b+D6KaB54K577yaAA=="
	.align	3
.LC37:
	.base64	"ICDkuI3mmK8i5qCI5b+r5Li65LuA5LmI5LiN5YWo55So5qCIIgA="
	.align	3
.LC38:
	.base64	"ICDogIzmmK8i5LuA5LmI5Zy65pmv55So5LuA5LmIIgoA"
	.align	3
.LC39:
	.base64	"ICAtIOWwj+WvueixoeOAgeefreeUn+WRveWRqOacnyDihpIg5qCI77yI5oCn6IO95pyA5LyY77yJAA=="
	.align	3
.LC40:
	.base64	"ICAtIOWkp+WvueixoeOAgei3qOWHveaVsOOAgeWKqOaAgeWkp+WwjyDihpIg5aCG77yI5Yqf6IO96ZyA5rGC77yJAA=="
	.align	3
.LC41:
	.base64	"ICAtIOWvueixoeWkjeeUqOOAgeWGheWtmOaxoCDihpIg5aCG5Lmf5Y+v5Lul5b6I5b+rAA=="
	.section	.text.startup,"ax",@progbits
	.align	2
	.p2align 5,,15
/* main：puts 标题与说明 → run_test×4 → puts 结论段落 */
	.global	main
	.type	main, %function
main:
.LFB13:
	.cfi_startproc
	stp	x29, x30, [sp, -48]!	//,,,
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -32
	.cfi_offset 20, -24
// int main() { — 横幅与说明
	adrp	x19, .LC9	// tmp103,
	add	x0, x19, :lo12:.LC9	//, tmp103,
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -16
	bl	puts		//
	adrp	x0, .LC10	// tmp105,
	add	x0, x0, :lo12:.LC10	//, tmp105,
	bl	puts		//
	adrp	x20, .LC13	// tmp118,
	add	x0, x19, :lo12:.LC9	//, tmp103,
	bl	puts		//
	adrp	x0, .LC11	// tmp109,
	add	x0, x0, :lo12:.LC11	//, tmp109,
	bl	puts		//
// run_test 场景1
	adrp	x21, test1_stack_small_object	// tmp116,
	mov	w5, 16960	//,
	add	x2, x21, :lo12:test1_stack_small_object	//, tmp116,
	add	x1, x20, :lo12:.LC13	//, tmp118,
	movk	w5, 0xf, lsl 16	//,,
	adrp	x4, test1_heap_small_object	// tmp112,
	adrp	x3, .LC12	// tmp114,
	add	x4, x4, :lo12:test1_heap_small_object	//, tmp112,
	add	x3, x3, :lo12:.LC12	//, tmp114,
	adrp	x0, .LC14	// tmp120,
	add	x0, x0, :lo12:.LC14	//, tmp120,
	bl	run_test		//
// run_test 场景2
	mov	w5, 16960	//,
	adrp	x4, test2_heap_reuse	// tmp123,
	movk	w5, 0xf, lsl 16	//,,
	add	x4, x4, :lo12:test2_heap_reuse	//, tmp123,
	adrp	x3, .LC15	// tmp125,
	adrp	x2, test2_stack_with_function_call	// tmp127,
	add	x3, x3, :lo12:.LC15	//, tmp125,
	add	x2, x2, :lo12:test2_stack_with_function_call	//, tmp127,
	adrp	x1, .LC16	// tmp129,
	adrp	x0, .LC17	// tmp131,
	add	x1, x1, :lo12:.LC16	//, tmp129,
	add	x0, x0, :lo12:.LC17	//, tmp131,
	bl	run_test		//
// run_test 场景3
	mov	w5, 100	//,
	adrp	x4, test3_heap_large_object	// tmp133,
	adrp	x3, .LC18	// tmp135,
	add	x4, x4, :lo12:test3_heap_large_object	//, tmp133,
	add	x3, x3, :lo12:.LC18	//, tmp135,
	adrp	x2, test3_stack_medium_object	// tmp137,
	adrp	x1, .LC19	// tmp139,
	add	x2, x2, :lo12:test3_stack_medium_object	//, tmp137,
	add	x1, x1, :lo12:.LC19	//, tmp139,
	adrp	x0, .LC20	// tmp141,
	add	x0, x0, :lo12:.LC20	//, tmp141,
	bl	run_test		//
// run_test 场景4
	add	x2, x21, :lo12:test1_stack_small_object	//, tmp116,
	add	x1, x20, :lo12:.LC13	//, tmp118,
	mov	w5, 16960	//,
	adrp	x4, test4_heap_with_pool	// tmp144,
	adrp	x3, .LC21	// tmp146,
	add	x4, x4, :lo12:test4_heap_with_pool	//, tmp144,
	add	x3, x3, :lo12:.LC21	//, tmp146,
	movk	w5, 0xf, lsl 16	//,,
	adrp	x0, .LC22	// tmp152,
	add	x0, x0, :lo12:.LC22	//, tmp152,
	bl	run_test		//
// 结论与要点（连串 puts）
	adrp	x0, .LC23	// tmp154,
	add	x0, x0, :lo12:.LC23	//, tmp154,
	bl	puts		//
	adrp	x0, .LC24	// tmp156,
	add	x0, x0, :lo12:.LC24	//, tmp156,
	bl	puts		//
	add	x0, x19, :lo12:.LC9	//, tmp103,
	bl	puts		//
	mov	w0, 10	//,
	bl	putchar		//
	adrp	x0, .LC25	// tmp160,
	add	x0, x0, :lo12:.LC25	//, tmp160,
	bl	puts		//
	adrp	x0, .LC26	// tmp162,
	add	x0, x0, :lo12:.LC26	//, tmp162,
	bl	puts		//
	adrp	x0, .LC27	// tmp164,
	add	x0, x0, :lo12:.LC27	//, tmp164,
	bl	puts		//
	adrp	x0, .LC28	// tmp166,
	add	x0, x0, :lo12:.LC28	//, tmp166,
	bl	puts		//
	adrp	x0, .LC29	// tmp168,
	add	x0, x0, :lo12:.LC29	//, tmp168,
	bl	puts		//
	adrp	x0, .LC30	// tmp170,
	add	x0, x0, :lo12:.LC30	//, tmp170,
	bl	puts		//
	adrp	x0, .LC31	// tmp172,
	add	x0, x0, :lo12:.LC31	//, tmp172,
	bl	puts		//
	adrp	x0, .LC32	// tmp174,
	add	x0, x0, :lo12:.LC32	//, tmp174,
	bl	puts		//
	adrp	x0, .LC33	// tmp176,
	add	x0, x0, :lo12:.LC33	//, tmp176,
	bl	puts		//
	adrp	x0, .LC34	// tmp178,
	add	x0, x0, :lo12:.LC34	//, tmp178,
	bl	puts		//
	adrp	x0, .LC35	// tmp180,
	add	x0, x0, :lo12:.LC35	//, tmp180,
	bl	puts		//
	adrp	x0, .LC36	// tmp182,
	add	x0, x0, :lo12:.LC36	//, tmp182,
	bl	puts		//
	adrp	x0, .LC37	// tmp184,
	add	x0, x0, :lo12:.LC37	//, tmp184,
	bl	puts		//
	adrp	x0, .LC38	// tmp186,
	add	x0, x0, :lo12:.LC38	//, tmp186,
	bl	puts		//
	adrp	x0, .LC39	// tmp188,
	add	x0, x0, :lo12:.LC39	//, tmp188,
	bl	puts		//
	adrp	x0, .LC40	// tmp190,
	add	x0, x0, :lo12:.LC40	//, tmp190,
	bl	puts		//
	adrp	x0, .LC41	// tmp192,
	add	x0, x0, :lo12:.LC41	//, tmp192,
	bl	puts		//
	mov	w0, 10	//,
	bl	putchar		//
// }
	ldr	x21, [sp, 32]	//,
	mov	w0, 0	//,
	ldp	x19, x20, [sp, 16]	//,,
	ldp	x29, x30, [sp], 48	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.cfi_endproc
.LFE13:
	.size	main, .-main
/* g_sum：volatile 全局，循环经 .LANCHOR0 引用 */
	.global	g_sum
	.section	.rodata.cst16,"aM",@progbits,16
	.align	4
.LC0:
	.xword	1024
	.xword	1000
	.bss
	.align	2
	.set	.LANCHOR0,. + 0
	.type	g_sum, %object
	.size	g_sum, 4
g_sum:
	.zero	4
	.ident	"GCC: (Alpine 15.2.0) 15.2.0"
	.section	.note.GNU-stack,"",@progbits
```