	.arch armv8-a
	.file	"heap_vs_stack_fairness.c"
/* ============================================================================
 * 完整汇编（Linux 容器）+ 人工标注
 *
 * 生成命令（仓库根目录）：
 *   docker-compose run --rm benchmark sh -c \
 *     'gcc -S -O2 -fverbose-asm demos/heap_vs_stack_fairness.c \
 *      -o demos/heap_vs_stack_fairness.linux_container_O2.s'
 *
 * 环境：Alpine GCC 15.2.0，aarch64-alpine-linux-musl，-O2，默认栈保护。
 * 标注说明：以 "【】" 标出热循环、syscall 边界、栈金丝雀、场景2 对比核心。
 * 配套文档：FAIRNESS_CONTAINER_LINUX_ASM_ANALYSIS.md
 * ============================================================================
 */
// GNU C23 (Alpine 15.2.0) version 15.2.0 (aarch64-alpine-linux-musl)
//	compiled by GNU C version 15.2.0, GMP version 6.3.0, MPFR version 4.2.2, MPC version 1.3.1, isl version isl-0.26-GMP

// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed: -march=armv8-a -mlittle-endian -mabi=lp64 -O2
	.text
	.align	2
	.p2align 5,,15
/* 【场景1-栈】test1_stack_small_object：函数级 sub sp,#1072（1KB buffer + canary 槽）。
 * 热循环 .L2：对 [sp+40]、[sp+1063] 读写；无 bl。计时含 clock_gettime。
 */
	.global	test1_stack_small_object
	.type	test1_stack_small_object, %function
test1_stack_small_object:
.LFB0:
	.cfi_startproc
	stp	x29, x30, [sp, -16]!	//,,,
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	adrp	x0, :got:__stack_chk_guard	// tmp122,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp122,
	mov	x29, sp	//,
	sub	sp, sp, #1072	//,,
	.cfi_def_cfa_offset 1088
// demos/heap_vs_stack_fairness.c:30: uint64_t test1_stack_small_object() {
	ldr	x1, [x0]	// tmp149,
	str	x1, [sp, 1064]	// tmp149, D.5489
	mov	x1, 0	// tmp149
// demos/heap_vs_stack_fairness.c:32:     clock_gettime(CLOCK_MONOTONIC, &start);
	mov	w0, 1	//,
	add	x1, sp, 8	//,,
	bl	clock_gettime		//
	adrp	x3, .LANCHOR0	// tmp148,
// demos/heap_vs_stack_fairness.c:34:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w5, 16960	// tmp136,
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	x3, x3, :lo12:.LANCHOR0	// tmp131, tmp148,
// demos/heap_vs_stack_fairness.c:34:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w1, 0	// i,
// demos/heap_vs_stack_fairness.c:34:     for (int i = 0; i < ITERATIONS; i++) {
	movk	w5, 0xf, lsl 16	// tmp136,,
	.p2align 5,,15
/* 【热循环 .L2】场景1-栈：百万次迭代，仅访存与 g_sum，无函数调用。 */
.L2:
// demos/heap_vs_stack_fairness.c:39:         p[0] = i & 0xFF;
	strb	w1, [sp, 40]	// _128, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:40:         p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
	asr	w0, w1, 8	// _3, i,
	strb	w0, [sp, 1063]	// _3, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:34:     for (int i = 0; i < ITERATIONS; i++) {
	add	w1, w1, 1	// i, i,
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w0, [sp, 40]	//, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w2, [sp, 1063]	//, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldr	w4, [x3]	//, g_sum
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	and	w2, w2, 255	// _6, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w0, w2, w0, uxtb	// _29, _6, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:41:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w0, w0, w4	// _9, _29, g_sum.0_8
	str	w0, [x3]	// _9, g_sum
// demos/heap_vs_stack_fairness.c:34:     for (int i = 0; i < ITERATIONS; i++) {
	cmp	w1, w5	// i, tmp136
	bne	.L2		//,
// demos/heap_vs_stack_fairness.c:44:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:46:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:45:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:46:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:45:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp142,
	movk	x2, 0x3b9a, lsl 16	// tmp142,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp142, _17
// demos/heap_vs_stack_fairness.c:47: }
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
/* 【场景3-栈】test3_stack_medium_object：256KB 局部区（大 sub sp），仅 100 次迭代。 */
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
// demos/heap_vs_stack_fairness.c:128: uint64_t test3_stack_medium_object() {
	add	x1, sp, 262144	// tmp149,,
	ldr	x2, [x0]	// tmp144,
	str	x2, [x1, 40]	// tmp144, D.5503
	mov	x2, 0	// tmp144
// demos/heap_vs_stack_fairness.c:130:     clock_gettime(CLOCK_MONOTONIC, &start);
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	adrp	x3, .LANCHOR0	// tmp143,
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	add	x3, x3, :lo12:.LANCHOR0	// tmp127, tmp143,
// demos/heap_vs_stack_fairness.c:134:     for (int i = 0; i < 100; i++) {  // 只100次，避免栈压力
	mov	w1, 0	// i,
	.p2align 5,,15
.L9:
// demos/heap_vs_stack_fairness.c:139:         p[MEDIUM_SIZE-1] = (i >> 8) & 0xFF;
	add	x0, sp, 262144	// tmp152,,
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	add	x2, sp, 262144	// tmp154,,
// demos/heap_vs_stack_fairness.c:138:         p[0] = i & 0xFF;
	strb	w1, [sp, 40]	// _110, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:134:     for (int i = 0; i < 100; i++) {  // 只100次，避免栈压力
	add	w1, w1, 1	// i, i,
// demos/heap_vs_stack_fairness.c:139:         p[MEDIUM_SIZE-1] = (i >> 8) & 0xFF;
	strb	wzr, [x0, 39]	//, MEM[(volatile char *)&buffer + 262143B]
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	ldrb	w0, [sp, 40]	//, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	ldrb	w2, [x2, 39]	//, MEM[(volatile char *)&buffer + 262143B]
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	ldr	w4, [x3]	//, g_sum
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	and	w2, w2, 255	// _4, MEM[(volatile char *)&buffer + 262143B]
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	add	w0, w2, w0, uxtb	// _27, _4, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:140:         g_sum += p[0] + p[MEDIUM_SIZE-1];
	add	w0, w0, w4	// _7, _27, g_sum.4_6
	str	w0, [x3]	// _7, g_sum
// demos/heap_vs_stack_fairness.c:134:     for (int i = 0; i < 100; i++) {  // 只100次，避免栈压力
	cmp	w1, 100	// i,
	bne	.L9		//,
// demos/heap_vs_stack_fairness.c:143:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:146:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:147: }
	add	x3, sp, 262144	// tmp156,,
// demos/heap_vs_stack_fairness.c:145:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _10, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:146:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _15, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:145:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp137,
	movk	x2, 0x3b9a, lsl 16	// tmp137,,
	madd	x0, x0, x2, x1	// <retval>, _10, tmp137, _15
// demos/heap_vs_stack_fairness.c:147: }
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
/* 【场景1-堆】test1_heap_small_object：每轮 bl malloc / bl free（内核 mmap/munmap 压力）。 */
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
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	x20, x20, :lo12:.LANCHOR0	// tmp132, tmp149,
// demos/heap_vs_stack_fairness.c:49: uint64_t test1_heap_small_object() {
	str	x21, [sp, 80]	//,
	.cfi_offset 21, -16
// demos/heap_vs_stack_fairness.c:53:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w21, 16960	// tmp137,
// demos/heap_vs_stack_fairness.c:53:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w19, 0	// i,
// demos/heap_vs_stack_fairness.c:53:     for (int i = 0; i < ITERATIONS; i++) {
	movk	w21, 0xf, lsl 16	// tmp137,,
// demos/heap_vs_stack_fairness.c:49: uint64_t test1_heap_small_object() {
	ldr	x1, [x0]	// tmp152,
	str	x1, [sp, 40]	// tmp152, D.5517
	mov	x1, 0	// tmp152
// demos/heap_vs_stack_fairness.c:51:     clock_gettime(CLOCK_MONOTONIC, &start);
	mov	w0, 1	//,
	add	x1, sp, 8	//,,
	bl	clock_gettime		//
	.p2align 5,,15
.L15:
// demos/heap_vs_stack_fairness.c:54:         char *buffer = malloc(SMALL_SIZE);
	mov	x0, 1024	//,
	bl	malloc		//
// demos/heap_vs_stack_fairness.c:58:         p[0] = i & 0xFF;
	strb	w19, [x0]	// _130, MEM[(volatile char *)buffer_28]
// demos/heap_vs_stack_fairness.c:59:         p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
	asr	w2, w19, 8	// _3, i,
	strb	w2, [x0, 1023]	// _3, MEM[(volatile char *)buffer_28 + 1023B]
// demos/heap_vs_stack_fairness.c:53:     for (int i = 0; i < ITERATIONS; i++) {
	add	w19, w19, 1	// i, i,
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w2, [x0]	//, MEM[(volatile char *)buffer_28]
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w1, [x0, 1023]	//, MEM[(volatile char *)buffer_28 + 1023B]
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldr	w3, [x20]	//, g_sum
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	and	w1, w1, 255	// _6, MEM[(volatile char *)buffer_28 + 1023B]
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w1, w2, uxtb	// _31, _6, MEM[(volatile char *)buffer_28]
// demos/heap_vs_stack_fairness.c:60:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w1, w3	// _9, _31, g_sum.1_8
	str	w1, [x20]	// _9, g_sum
// demos/heap_vs_stack_fairness.c:62:         free(buffer);
	bl	free		//
// demos/heap_vs_stack_fairness.c:53:     for (int i = 0; i < ITERATIONS; i++) {
	cmp	w19, w21	// i, tmp137
	bne	.L15		//,
// demos/heap_vs_stack_fairness.c:65:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:67:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:66:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:67:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:66:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp143,
	movk	x2, 0x3b9a, lsl 16	// tmp143,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp143, _17
// demos/heap_vs_stack_fairness.c:68: }
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
/* 【场景2-堆】test2_heap_reuse：
 * 【边界】bl malloc、第一次 bl clock_gettime 在热循环 .L21 之前；
 *         第二次 clock_gettime 之后才是 bl free（计时区间不含 malloc/free）。
 * 【热循环 .L21】x19=堆指针，str/ldr [x19] 与 [x19,#1023]；无 bl 子函数。
 */
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
// demos/heap_vs_stack_fairness.c:98: uint64_t test2_heap_reuse() {
	ldr	x1, [x0]	// tmp151,
	str	x1, [sp, 40]	// tmp151, D.5531
	mov	x1, 0	// tmp151
// demos/heap_vs_stack_fairness.c:102:     char *buffer = malloc(SMALL_SIZE);
	mov	x0, 1024	//,
	bl	malloc		//
	mov	x19, x0	// buffer, buffer
// demos/heap_vs_stack_fairness.c:104:     clock_gettime(CLOCK_MONOTONIC, &start);
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	adrp	x3, .LANCHOR0	// tmp149,
// demos/heap_vs_stack_fairness.c:106:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w5, 16960	// tmp137,
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	x3, x3, :lo12:.LANCHOR0	// tmp132, tmp149,
// demos/heap_vs_stack_fairness.c:106:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w2, 0	// i,
// demos/heap_vs_stack_fairness.c:106:     for (int i = 0; i < ITERATIONS; i++) {
	movk	w5, 0xf, lsl 16	// tmp137,,
	.p2align 5,,15
/* 【热循环 .L21】场景2-堆复用：计时核心；x19 为 malloc 得到的指针。 */
.L21:
// demos/heap_vs_stack_fairness.c:109:         p[0] = i & 0xFF;
	strb	w2, [x19]	// _130, MEM[(volatile char *)buffer_23]
// demos/heap_vs_stack_fairness.c:110:         p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
	asr	w0, w2, 8	// _3, i,
	strb	w0, [x19, 1023]	// _3, MEM[(volatile char *)buffer_23 + 1023B]
// demos/heap_vs_stack_fairness.c:106:     for (int i = 0; i < ITERATIONS; i++) {
	add	w2, w2, 1	// i, i,
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w1, [x19]	//, MEM[(volatile char *)buffer_23]
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w0, [x19, 1023]	//, MEM[(volatile char *)buffer_23 + 1023B]
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldr	w4, [x3]	//, g_sum
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	and	w0, w0, 255	// _6, MEM[(volatile char *)buffer_23 + 1023B]
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w0, w1, uxtb	// _32, _6, MEM[(volatile char *)buffer_23]
// demos/heap_vs_stack_fairness.c:111:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w1, w4	// _9, _32, g_sum.3_8
	str	w1, [x3]	// _9, g_sum
// demos/heap_vs_stack_fairness.c:106:     for (int i = 0; i < ITERATIONS; i++) {
	cmp	w2, w5	// i, tmp137
	bne	.L21		//,
// demos/heap_vs_stack_fairness.c:114:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:116:     free(buffer);
	mov	x0, x19	//, buffer
	bl	free		//
// demos/heap_vs_stack_fairness.c:119:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:118:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:119:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:118:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp143,
	movk	x2, 0x3b9a, lsl 16	// tmp143,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp143, _17
// demos/heap_vs_stack_fairness.c:120: }
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
/* 【场景3-堆】test3_heap_large_object：每轮 1MB malloc + 使用 + free。 */
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
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	adrp	x20, .LANCHOR0	// tmp147,
	add	x20, x20, :lo12:.LANCHOR0	// tmp148, tmp147,
// demos/heap_vs_stack_fairness.c:154:     for (int i = 0; i < 100; i++) {
	mov	w19, 0	// i,
// demos/heap_vs_stack_fairness.c:150: uint64_t test3_heap_large_object() {
	ldr	x1, [x0]	// tmp150,
	str	x1, [sp, 40]	// tmp150, D.5545
	mov	x1, 0	// tmp150
// demos/heap_vs_stack_fairness.c:152:     clock_gettime(CLOCK_MONOTONIC, &start);
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	.p2align 5,,15
.L28:
// demos/heap_vs_stack_fairness.c:155:         char *buffer = malloc(LARGE_SIZE);  // 1MB，没问题
	mov	x0, 1048576	//,
	bl	malloc		//
// demos/heap_vs_stack_fairness.c:156:         if (buffer) {
	cbz	x0, .L27	// tmp123,
// demos/heap_vs_stack_fairness.c:160:             p[LARGE_SIZE-1] = (i >> 8) & 0xFF;
	add	x2, x0, 1044480	// tmp125, tmp123,
// demos/heap_vs_stack_fairness.c:159:             p[0] = i & 0xFF;
	strb	w19, [x0]	// _113, MEM[(volatile char *)buffer_27]
// demos/heap_vs_stack_fairness.c:160:             p[LARGE_SIZE-1] = (i >> 8) & 0xFF;
	strb	wzr, [x2, 4095]	//, MEM[(volatile char *)buffer_27 + 1048575B]
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	ldrb	w1, [x0]	//, MEM[(volatile char *)buffer_27]
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	ldrb	w2, [x2, 4095]	//, MEM[(volatile char *)buffer_27 + 1048575B]
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	ldr	w3, [x20]	//, g_sum
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	and	w2, w2, 255	// _4, MEM[(volatile char *)buffer_27 + 1048575B]
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	add	w1, w2, w1, uxtb	// _30, _4, MEM[(volatile char *)buffer_27]
// demos/heap_vs_stack_fairness.c:161:             g_sum += p[0] + p[LARGE_SIZE-1];
	add	w1, w1, w3	// _7, _30, g_sum.5_6
	str	w1, [x20]	// _7, g_sum
// demos/heap_vs_stack_fairness.c:163:             free(buffer);
	bl	free		//
.L27:
// demos/heap_vs_stack_fairness.c:154:     for (int i = 0; i < 100; i++) {
	add	w19, w19, 1	// i, i,
// demos/heap_vs_stack_fairness.c:154:     for (int i = 0; i < 100; i++) {
	cmp	w19, 100	// i,
	bne	.L28		//,
// demos/heap_vs_stack_fairness.c:167:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:169:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:168:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _10, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:169:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _15, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:168:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp140,
	movk	x2, 0x3b9a, lsl 16	// tmp140,,
	madd	x0, x0, x2, x1	// <retval>, _10, tmp140, _15
// demos/heap_vs_stack_fairness.c:170: }
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
/* 【场景4-堆】test4_heap_with_pool：计时前 pool_create；热循环内 bl pool_alloc。 */
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
// demos/heap_vs_stack_fairness.c:207: uint64_t test4_heap_with_pool() {
	ldr	x1, [x0]	// tmp171,
	str	x1, [sp, 40]	// tmp171, D.5569
	mov	x1, 0	// tmp171
// demos/heap_vs_stack_fairness.c:185:     MemoryPool *pool = malloc(sizeof(MemoryPool));
	mov	x0, 32	//,
	bl	malloc		//
	mov	x19, x0	// pool, pool
// demos/heap_vs_stack_fairness.c:186:     pool->memory = malloc(block_size * num_blocks);
	mov	x0, 40960	//,
	movk	x0, 0xf, lsl 16	//,,
	bl	malloc		//
// demos/heap_vs_stack_fairness.c:186:     pool->memory = malloc(block_size * num_blocks);
	str	x0, [x19]	// tmp170, pool_46->memory
// demos/heap_vs_stack_fairness.c:187:     pool->block_size = block_size;
	adrp	x0, .LC0	// tmp176,
// demos/heap_vs_stack_fairness.c:189:     pool->next_free = 0;
	str	xzr, [x19, 24]	//, pool_46->next_free
// demos/heap_vs_stack_fairness.c:213:     clock_gettime(CLOCK_MONOTONIC, &start);
	add	x1, sp, 8	//,,
// demos/heap_vs_stack_fairness.c:187:     pool->block_size = block_size;
	ldr	q31, [x0, #:lo12:.LC0]	// tmp136,
// demos/heap_vs_stack_fairness.c:213:     clock_gettime(CLOCK_MONOTONIC, &start);
	mov	w0, 1	//,
// demos/heap_vs_stack_fairness.c:187:     pool->block_size = block_size;
	str	q31, [x19, 8]	// tmp136, MEM <vector(2) long unsigned int> [(long unsigned int *)pool_46 + 8B]
// demos/heap_vs_stack_fairness.c:213:     clock_gettime(CLOCK_MONOTONIC, &start);
	bl	clock_gettime		//
	adrp	x4, .LANCHOR0	// tmp166,
// demos/heap_vs_stack_fairness.c:215:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w6, 16960	// tmp153,
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	x4, x4, :lo12:.LANCHOR0	// tmp148, tmp166,
// demos/heap_vs_stack_fairness.c:215:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w0, 0	// i,
// demos/heap_vs_stack_fairness.c:215:     for (int i = 0; i < ITERATIONS; i++) {
	movk	w6, 0xf, lsl 16	// tmp153,,
	.p2align 5,,15
.L38:
// demos/heap_vs_stack_fairness.c:194:     if (pool->next_free >= pool->total_blocks) {
	ldp	x2, x1, [x19, 16]	// pool_46->total_blocks, _42,
// demos/heap_vs_stack_fairness.c:198:     pool->next_free++;
	add	x5, x1, 1	// _63, _42,
// demos/heap_vs_stack_fairness.c:194:     if (pool->next_free >= pool->total_blocks) {
	cmp	x1, x2	// _42, pool_46->total_blocks
	bcc	.L37		//,
	mov	x5, 1	// _63,
	mov	x1, 0	// _42,
.L37:
// demos/heap_vs_stack_fairness.c:197:     void *ptr = pool->memory + (pool->next_free * pool->block_size);
	ldp	x2, x3, [x19]	// pool_46->memory, pool_46->block_size,* pool
// demos/heap_vs_stack_fairness.c:198:     pool->next_free++;
	str	x5, [x19, 24]	// _63, pool_46->next_free
// demos/heap_vs_stack_fairness.c:220:         p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
	asr	w5, w0, 8	// _74, i,
// demos/heap_vs_stack_fairness.c:197:     void *ptr = pool->memory + (pool->next_free * pool->block_size);
	mul	x1, x1, x3	// _68, _42, pool_46->block_size
// demos/heap_vs_stack_fairness.c:197:     void *ptr = pool->memory + (pool->next_free * pool->block_size);
	add	x3, x2, x1	// ptr, pool_46->memory, _68
// demos/heap_vs_stack_fairness.c:219:         p[0] = i & 0xFF;
	strb	w0, [x2, x1]	// _7, MEM[(volatile char *)ptr_69]
// demos/heap_vs_stack_fairness.c:215:     for (int i = 0; i < ITERATIONS; i++) {
	add	w0, w0, 1	// i, i,
// demos/heap_vs_stack_fairness.c:220:         p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
	strb	w5, [x3, 1023]	// _74, MEM[(volatile char *)ptr_69 + 1023B]
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w1, [x2, x1]	//, MEM[(volatile char *)ptr_69]
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w2, [x3, 1023]	//, MEM[(volatile char *)ptr_69 + 1023B]
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	ldr	w3, [x4]	//, g_sum
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	and	w2, w2, 255	// _78, MEM[(volatile char *)ptr_69 + 1023B]
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w2, w1, uxtb	// _80, _78, MEM[(volatile char *)ptr_69]
// demos/heap_vs_stack_fairness.c:221:         g_sum += p[0] + p[SMALL_SIZE-1];
	add	w1, w1, w3	// _82, _80, g_sum.6_81
	str	w1, [x4]	// _82, g_sum
// demos/heap_vs_stack_fairness.c:215:     for (int i = 0; i < ITERATIONS; i++) {
	cmp	w0, w6	// i, tmp153
	bne	.L38		//,
// demos/heap_vs_stack_fairness.c:226:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:203:     free(pool->memory);
	ldr	x0, [x19]	//, pool_46->memory
	bl	free		//
// demos/heap_vs_stack_fairness.c:204:     free(pool);
	mov	x0, x19	//, pool
	bl	free		//
// demos/heap_vs_stack_fairness.c:231:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:230:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _12, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:231:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _17, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:230:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp160,
	movk	x2, 0x3b9a, lsl 16	// tmp160,,
	madd	x0, x0, x2, x1	// <retval>, _12, tmp160, _17
// demos/heap_vs_stack_fairness.c:232: }
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
 * 【栈帧】sub sp,sp,#1040 — 对应 char buffer[1024] + 金丝雀槽。
 * 【栈保护】序言：GOT 取 __stack_chk_guard，副本存 [sp,#1032]；尾声前比较，失败 bl __stack_chk_fail。
 * 每被调用一次执行上述全套（与场景2 外层循环次数相同）。
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
/* 【guard-步骤1】经 GOT 取 __stack_chk_guard 的地址（变量，不是函数调用）。 */
	adrp	x1, :got:__stack_chk_guard	// tmp112,
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]	// tmp112,
	mov	x29, sp	//,
	sub	sp, sp, #1040	//,,
	.cfi_def_cfa_offset 1056
// demos/heap_vs_stack_fairness.c:75: void process_with_stack_buffer(int value) {
/* 【guard-步骤2】读取当前 canary 值并保存到当前栈帧槽位 [sp,#1032]。 */
	ldr	x2, [x1]	// tmp127,
	str	x2, [sp, 1032]	// tmp127, D.5573
	mov	x2, 0	// tmp127
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	adrp	x2, .LANCHOR0	// tmp121,
// demos/heap_vs_stack_fairness.c:79:     p[0] = value & 0xFF;
	strb	w0, [sp, 8]	// _1, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:80:     p[SMALL_SIZE-1] = (value >> 8) & 0xFF;
	asr	w0, w0, 8	// _3, value,
	strb	w0, [sp, 1031]	// _3, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w0, [sp, 8]	//, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	ldrb	w1, [sp, 1031]	//, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	ldr	w3, [x2, #:lo12:.LANCHOR0]	//, g_sum
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	and	w1, w1, 255	// _6, MEM[(volatile char *)&buffer + 1023B]
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	add	w0, w1, w0, uxtb	// _14, _6, MEM[(volatile char *)&buffer]
// demos/heap_vs_stack_fairness.c:81:     g_sum += p[0] + p[SMALL_SIZE-1];
	add	w0, w0, w3	// _9, _14, g_sum.2_8
	str	w0, [x2, #:lo12:.LANCHOR0]	// _9, g_sum
// demos/heap_vs_stack_fairness.c:82: }
/* 【guard-步骤3】函数返回前再取一次 guard，与栈中副本比较。 */
	adrp	x0, :got:__stack_chk_guard	// tmp125,
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]	// tmp125,
	ldr	x2, [sp, 1032]	// tmp128, D.5573
	ldr	x1, [x0]	// tmp129,
	subs	x2, x2, x1	// tmp128, tmp129
	mov	x1, 0	// tmp129
/* 【guard-步骤4】不相等说明栈被破坏，进入失败路径。 */
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
/* 【guard-失败处理】调用 musl 的 __stack_chk_fail（实现里是 a_crash）。 */
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE2:
	.size	process_with_stack_buffer, .-process_with_stack_buffer
	.align	2
	.p2align 5,,15
/* 【场景2-栈-外层】test2_stack_with_function_call：
 * 【热循环 .L49】每轮：mov 参数 w0、add i、bl process_with_stack_buffer — 百万次调用+返回。
 * 计时段从第一次 clock_gettime 后到第二次 clock_gettime，包含全部 bl。
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
// demos/heap_vs_stack_fairness.c:88:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w20, 16960	// tmp114,
// demos/heap_vs_stack_fairness.c:88:     for (int i = 0; i < ITERATIONS; i++) {
	mov	w19, 0	// i,
// demos/heap_vs_stack_fairness.c:88:     for (int i = 0; i < ITERATIONS; i++) {
	movk	w20, 0xf, lsl 16	// tmp114,,
// demos/heap_vs_stack_fairness.c:84: uint64_t test2_stack_with_function_call() {
/* 【函数级 guard】外层函数也保存自己的 canary 副本到 [sp,#40]。 */
	ldr	x1, [x0]	// tmp126,
	str	x1, [sp, 40]	// tmp126, D.5584
	mov	x1, 0	// tmp126
// demos/heap_vs_stack_fairness.c:86:     clock_gettime(CLOCK_MONOTONIC, &start);
	add	x1, sp, 8	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	.p2align 5,,15
/* 【热循环 .L49】场景2-栈：每轮 bl process_with_stack_buffer（见上方子函数标注）。 */
.L49:
// demos/heap_vs_stack_fairness.c:89:         process_with_stack_buffer(i);  // 每次调用函数
	mov	w0, w19	//, i
// demos/heap_vs_stack_fairness.c:88:     for (int i = 0; i < ITERATIONS; i++) {
	add	w19, w19, 1	// i, i,
// demos/heap_vs_stack_fairness.c:89:         process_with_stack_buffer(i);  // 每次调用函数
	bl	process_with_stack_buffer		//
// demos/heap_vs_stack_fairness.c:88:     for (int i = 0; i < ITERATIONS; i++) {
	cmp	w19, w20	// i, tmp114
	bne	.L49		//,
// demos/heap_vs_stack_fairness.c:92:     clock_gettime(CLOCK_MONOTONIC, &end);
	add	x1, sp, 24	//,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// demos/heap_vs_stack_fairness.c:94:            (end.tv_nsec - start.tv_nsec);
	ldp	x1, x2, [sp, 8]	// start.tv_sec, start.tv_nsec,
// demos/heap_vs_stack_fairness.c:93:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	ldr	x0, [sp, 24]	// end.tv_sec, end.tv_sec
	sub	x0, x0, x1	// _3, end.tv_sec, start.tv_sec
// demos/heap_vs_stack_fairness.c:94:            (end.tv_nsec - start.tv_nsec);
	ldr	x1, [sp, 32]	// end.tv_nsec, end.tv_nsec
	sub	x1, x1, x2	// _8, end.tv_nsec, start.tv_nsec
// demos/heap_vs_stack_fairness.c:93:     return (end.tv_sec - start.tv_sec) * 1000000000ULL +
	mov	x2, 51712	// tmp120,
	movk	x2, 0x3b9a, lsl 16	// tmp120,,
	madd	x0, x0, x2, x1	// <retval>, _3, tmp120, _8
// demos/heap_vs_stack_fairness.c:95: }
/* 【函数级 guard 校验】比较 [sp,#40] 与当前 guard，失败走 .L53。 */
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
/* 【函数级 guard 失败】同样进入 __stack_chk_fail。 */
	bl	__stack_chk_fail		//
	.cfi_endproc
.LFE3:
	.size	test2_stack_with_function_call, .-test2_stack_with_function_call
	.align	2
	.p2align 5,,15
/* 【内存池】pool_create / pool_alloc / pool_destroy — 场景4 辅助。 */
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
// demos/heap_vs_stack_fairness.c:184: MemoryPool* pool_create(size_t block_size, size_t num_blocks) {
	mov	x21, x0	// block_size, block_size
// demos/heap_vs_stack_fairness.c:185:     MemoryPool *pool = malloc(sizeof(MemoryPool));
	mov	x0, 32	//,
	bl	malloc		//
	mov	x19, x0	// tmp106, tmp112
// demos/heap_vs_stack_fairness.c:186:     pool->memory = malloc(block_size * num_blocks);
	mul	x0, x21, x20	//, block_size, num_blocks
	bl	malloc		//
// demos/heap_vs_stack_fairness.c:186:     pool->memory = malloc(block_size * num_blocks);
	stp	x0, x21, [x19]	// tmp113, block_size,
// demos/heap_vs_stack_fairness.c:191: }
	mov	x0, x19	//, tmp106
// demos/heap_vs_stack_fairness.c:188:     pool->total_blocks = num_blocks;
	stp	x20, xzr, [x19, 16]	// num_blocks,,
// demos/heap_vs_stack_fairness.c:191: }
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
/* pool_alloc：O(1) 指针算术（madd），无堆分配 syscall。 */
	.global	pool_alloc
	.type	pool_alloc, %function
pool_alloc:
.LFB8:
	.cfi_startproc
// demos/heap_vs_stack_fairness.c:194:     if (pool->next_free >= pool->total_blocks) {
	ldp	x2, x1, [x0, 16]	// pool_10(D)->total_blocks, _1,
// demos/heap_vs_stack_fairness.c:198:     pool->next_free++;
	add	x4, x1, 1	// _18, _1,
// demos/heap_vs_stack_fairness.c:194:     if (pool->next_free >= pool->total_blocks) {
	cmp	x1, x2	// _1, pool_10(D)->total_blocks
	bcc	.L57		//,
	mov	x4, 1	// _18,
	mov	x1, 0	// _1,
.L57:
// demos/heap_vs_stack_fairness.c:197:     void *ptr = pool->memory + (pool->next_free * pool->block_size);
	ldp	x2, x3, [x0]	// pool_10(D)->memory, pool_10(D)->block_size,* pool
// demos/heap_vs_stack_fairness.c:198:     pool->next_free++;
	str	x4, [x0, 24]	// _18, pool_10(D)->next_free
// demos/heap_vs_stack_fairness.c:200: }
	madd	x0, x1, x3, x2	//, _1, pool_10(D)->block_size, pool_10(D)->memory
	ret	
	.cfi_endproc
.LFE8:
	.size	pool_alloc, .-pool_alloc
	.align	2
	.p2align 5,,15
/* pool_destroy：bl free 两次（pool->memory 与 pool 本体）。 */
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
// demos/heap_vs_stack_fairness.c:202: void pool_destroy(MemoryPool *pool) {
	mov	x19, x0	// pool, pool
// demos/heap_vs_stack_fairness.c:203:     free(pool->memory);
	ldr	x0, [x0]	//, pool_3(D)->memory
	bl	free		//
// demos/heap_vs_stack_fairness.c:204:     free(pool);
	mov	x0, x19	//, pool
// demos/heap_vs_stack_fairness.c:205: }
	ldr	x19, [sp, 16]	//,
	ldp	x29, x30, [sp], 32	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_def_cfa_offset 0
// demos/heap_vs_stack_fairness.c:204:     free(pool);
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
/* print_result：浮点除法后 b printf（格式化输出）。 */
	.global	print_result
	.type	print_result, %function
print_result:
.LFB11:
	.cfi_startproc
// demos/heap_vs_stack_fairness.c:239:     printf("  %-40s: %8.3f ms (平均 %6.1f ns/次)\n",
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
/* run_test：通过函数指针 blr 先后调用「栈测试」「堆测试」并打印比例。 */
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
// demos/heap_vs_stack_fairness.c:249:     printf("\n%s\n", title);
	mov	x1, x0	//, title
	adrp	x0, .LC2	// tmp120,
	add	x0, x0, :lo12:.LC2	//, tmp120,
// demos/heap_vs_stack_fairness.c:248:               int iterations) {
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
// demos/heap_vs_stack_fairness.c:248:               int iterations) {
	stp	x4, x2, [sp, 80]	// heap_test, stack_test,
// demos/heap_vs_stack_fairness.c:249:     printf("\n%s\n", title);
	bl	printf		//
// demos/heap_vs_stack_fairness.c:250:     printf("========================================\n");
	adrp	x0, .LC3	// tmp122,
	add	x0, x0, :lo12:.LC3	//, tmp122,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:252:     uint64_t stack_time = stack_test();
	ldr	x2, [sp, 88]	// stack_test, %sfp
	blr	x2		// stack_test
// demos/heap_vs_stack_fairness.c:239:     printf("  %-40s: %8.3f ms (平均 %6.1f ns/次)\n",
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
// demos/heap_vs_stack_fairness.c:255:     uint64_t heap_time = heap_test();
	ldr	x4, [sp, 80]	// heap_test, %sfp
	blr	x4		// heap_test
// demos/heap_vs_stack_fairness.c:239:     printf("  %-40s: %8.3f ms (平均 %6.1f ns/次)\n",
	ucvtf	d15, x0	// _26, heap_time
	mov	x1, x20	//, heap_name
	add	x0, x19, :lo12:.LC1	//, tmp128,
	fdiv	d0, d15, d13	//, _26, tmp125
	fdiv	d1, d15, d12	//, _26, _32
	bl	printf		//
// demos/heap_vs_stack_fairness.c:258:     double ratio = (double)heap_time / stack_time;
	fdiv	d15, d15, d14	// ratio, _26, _30
// demos/heap_vs_stack_fairness.c:259:     printf("  %-40s: %.2fx\n", "堆/栈比例", ratio);
	adrp	x1, .LC4	// tmp136,
	adrp	x0, .LC5	// tmp138,
	add	x1, x1, :lo12:.LC4	//, tmp136,
	add	x0, x0, :lo12:.LC5	//, tmp138,
	fmov	d0, d15	//, ratio
	bl	printf		//
// demos/heap_vs_stack_fairness.c:261:     if (ratio < 1.0) {
	fmov	d31, 1.0e+0	// tmp139,
	fcmpe	d15, d31	// ratio, tmp139
	bmi	.L67		//,
// demos/heap_vs_stack_fairness.c:263:     } else if (ratio < 2.0) {
	fmov	d30, 2.0e+0	// tmp142,
	fcmpe	d15, d30	// ratio, tmp142
	bmi	.L68		//,
// demos/heap_vs_stack_fairness.c:268: }
	ldr	x21, [sp, 32]	//,
// demos/heap_vs_stack_fairness.c:266:         printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
	fmov	d0, d15	//, ratio
// demos/heap_vs_stack_fairness.c:268: }
	ldp	x19, x20, [sp, 16]	//,,
// demos/heap_vs_stack_fairness.c:266:         printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
	adrp	x0, .LC8	// tmp151,
// demos/heap_vs_stack_fairness.c:268: }
	ldp	d12, d13, [sp, 48]	//,,
// demos/heap_vs_stack_fairness.c:266:         printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
	add	x0, x0, :lo12:.LC8	//, tmp151,
// demos/heap_vs_stack_fairness.c:268: }
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
// demos/heap_vs_stack_fairness.c:266:         printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
	b	printf		//
	.p2align 2,,3
.L68:
	.cfi_restore_state
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	fsub	d0, d15, d31	// _3, ratio, tmp139
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	mov	x0, 4636737291354636288	// tmp147,
// demos/heap_vs_stack_fairness.c:268: }
	ldr	x21, [sp, 32]	//,
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	fmov	d31, x0	// tmp161, tmp147
// demos/heap_vs_stack_fairness.c:268: }
	ldp	x19, x20, [sp, 16]	//,,
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	fmul	d0, d0, d31	//, _3, tmp161
// demos/heap_vs_stack_fairness.c:268: }
	ldp	d12, d13, [sp, 48]	//,,
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	adrp	x0, .LC7	// tmp149,
// demos/heap_vs_stack_fairness.c:268: }
	ldp	d14, d15, [sp, 64]	//,,
// demos/heap_vs_stack_fairness.c:264:         printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
	add	x0, x0, :lo12:.LC7	//, tmp149,
// demos/heap_vs_stack_fairness.c:268: }
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
// demos/heap_vs_stack_fairness.c:266:         printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
	b	printf		//
	.p2align 2,,3
.L67:
	.cfi_restore_state
// demos/heap_vs_stack_fairness.c:268: }
	ldr	x21, [sp, 32]	//,
// demos/heap_vs_stack_fairness.c:262:         printf("  ✓ 这个场景堆更快（或接近）\n");
	adrp	x0, .LC6	// tmp141,
// demos/heap_vs_stack_fairness.c:268: }
	ldp	x19, x20, [sp, 16]	//,,
// demos/heap_vs_stack_fairness.c:262:         printf("  ✓ 这个场景堆更快（或接近）\n");
	add	x0, x0, :lo12:.LC6	//, tmp141,
// demos/heap_vs_stack_fairness.c:268: }
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
// demos/heap_vs_stack_fairness.c:262:         printf("  ✓ 这个场景堆更快（或接近）\n");
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
/* main：四次 bl run_test；其中第二次 run_test 传入 test2_stack_with_function_call 与 test2_heap_reuse。 */
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
// demos/heap_vs_stack_fairness.c:271:     printf("======================================\n");
	adrp	x19, .LC9	// tmp103,
	add	x0, x19, :lo12:.LC9	//, tmp103,
// demos/heap_vs_stack_fairness.c:270: int main() {
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -16
// demos/heap_vs_stack_fairness.c:271:     printf("======================================\n");
	bl	puts		//
// demos/heap_vs_stack_fairness.c:272:     printf("公平的堆 vs 栈性能对比测试\n");
	adrp	x0, .LC10	// tmp105,
	add	x0, x0, :lo12:.LC10	//, tmp105,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:277:     run_test(
	adrp	x20, .LC13	// tmp118,
// demos/heap_vs_stack_fairness.c:273:     printf("======================================\n");
	add	x0, x19, :lo12:.LC9	//, tmp103,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:274:     printf("\n目标：展示各自适合的场景，而非简单的\"谁快用谁\"\n");
	adrp	x0, .LC11	// tmp109,
	add	x0, x0, :lo12:.LC11	//, tmp109,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:277:     run_test(
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
// demos/heap_vs_stack_fairness.c:287:     run_test(
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
// demos/heap_vs_stack_fairness.c:297:     run_test(
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
// demos/heap_vs_stack_fairness.c:307:     run_test(
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
// demos/heap_vs_stack_fairness.c:316:     printf("\n======================================\n");
	adrp	x0, .LC23	// tmp154,
	add	x0, x0, :lo12:.LC23	//, tmp154,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:317:     printf("结论\n");
	adrp	x0, .LC24	// tmp156,
	add	x0, x0, :lo12:.LC24	//, tmp156,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:318:     printf("======================================\n");
	add	x0, x19, :lo12:.LC9	//, tmp103,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:319:     printf("\n");
	mov	w0, 10	//,
	bl	putchar		//
// demos/heap_vs_stack_fairness.c:320:     printf("✓ 场景1：栈完胜（小对象短生命周期）\n");
	adrp	x0, .LC25	// tmp160,
	add	x0, x0, :lo12:.LC25	//, tmp160,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:321:     printf("  → 这是栈的优势场景\n\n");
	adrp	x0, .LC26	// tmp162,
	add	x0, x0, :lo12:.LC26	//, tmp162,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:323:     printf("⚖️ 场景2：堆可能更好（对象复用）\n");
	adrp	x0, .LC27	// tmp164,
	add	x0, x0, :lo12:.LC27	//, tmp164,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:324:     printf("  → 堆分配一次 vs 栈每次函数调用\n");
	adrp	x0, .LC28	// tmp166,
	add	x0, x0, :lo12:.LC28	//, tmp166,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:325:     printf("  → 取决于函数调用开销\n\n");
	adrp	x0, .LC29	// tmp168,
	add	x0, x0, :lo12:.LC29	//, tmp168,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:327:     printf("✓ 场景3：堆必需（大对象）\n");
	adrp	x0, .LC30	// tmp170,
	add	x0, x0, :lo12:.LC30	//, tmp170,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:328:     printf("  → 栈有大小限制（~8MB）\n");
	adrp	x0, .LC31	// tmp172,
	add	x0, x0, :lo12:.LC31	//, tmp172,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:329:     printf("  → 堆可以分配任意大小\n\n");
	adrp	x0, .LC32	// tmp174,
	add	x0, x0, :lo12:.LC32	//, tmp174,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:331:     printf("⚖️ 场景4：堆接近栈（内存池）\n");
	adrp	x0, .LC33	// tmp176,
	add	x0, x0, :lo12:.LC33	//, tmp176,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:332:     printf("  → 内存池优化后，堆可以很快\n");
	adrp	x0, .LC34	// tmp178,
	add	x0, x0, :lo12:.LC34	//, tmp178,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:333:     printf("  → tcmalloc/jemalloc更快\n\n");
	adrp	x0, .LC35	// tmp180,
	add	x0, x0, :lo12:.LC35	//, tmp180,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:335:     printf("核心要点：\n");
	adrp	x0, .LC36	// tmp182,
	add	x0, x0, :lo12:.LC36	//, tmp182,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:336:     printf("  不是\"栈快为什么不全用栈\"\n");
	adrp	x0, .LC37	// tmp184,
	add	x0, x0, :lo12:.LC37	//, tmp184,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:337:     printf("  而是\"什么场景用什么\"\n\n");
	adrp	x0, .LC38	// tmp186,
	add	x0, x0, :lo12:.LC38	//, tmp186,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:339:     printf("  - 小对象、短生命周期 → 栈（性能最优）\n");
	adrp	x0, .LC39	// tmp188,
	add	x0, x0, :lo12:.LC39	//, tmp188,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:340:     printf("  - 大对象、跨函数、动态大小 → 堆（功能需求）\n");
	adrp	x0, .LC40	// tmp190,
	add	x0, x0, :lo12:.LC40	//, tmp190,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:341:     printf("  - 对象复用、内存池 → 堆也可以很快\n");
	adrp	x0, .LC41	// tmp192,
	add	x0, x0, :lo12:.LC41	//, tmp192,
	bl	puts		//
// demos/heap_vs_stack_fairness.c:342:     printf("\n");
	mov	w0, 10	//,
	bl	putchar		//
// demos/heap_vs_stack_fairness.c:345: }
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
/* g_sum：volatile 防优化全局累加；测试循环通过 .LANCHOR0 / adrp 引用。 */
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
