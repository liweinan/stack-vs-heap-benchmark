CC = gcc
CFLAGS = -O2 -Wall -Wextra -g
LDFLAGS =

# 目标文件
TARGETS = stack_bench heap_bench mixed_bench stack_asm_demo stack_growth_comparison \
          stack_overflow_test stack_crash_demo stack_guard_page_demo stack_depth_tracer \
          pure_asm_stack_test

# 所有目标
all: $(TARGETS)

# 栈分配基准测试
stack_bench: src/stack_allocation.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 堆分配基准测试
heap_bench: src/heap_allocation.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 混合基准测试（对比）
mixed_bench: src/mixed_benchmark.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 汇编级演示
stack_asm_demo: src/stack_asm_demo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 栈增长模式对比测试（使用 -O0 -fno-inline 避免优化干扰缺页测试）
stack_growth_comparison: src/stack_growth_comparison.c
	$(CC) -O0 -Wall -Wextra -g -fno-inline -o $@ $< $(LDFLAGS)

# 栈溢出测试
stack_overflow_test: src/stack_overflow_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 栈崩溃演示
stack_crash_demo: src/stack_crash_demo.c
	$(CC) -O0 -Wall -Wextra -g -o $@ $< $(LDFLAGS)

# 栈保护页演示
stack_guard_page_demo: src/stack_guard_page_demo.c
	$(CC) -O0 -Wall -Wextra -g -pthread -o $@ $< $(LDFLAGS)

# 栈深度追踪器
stack_depth_tracer: src/stack_depth_tracer.c
	$(CC) -O0 -Wall -Wextra -g -o $@ $< $(LDFLAGS)

# 纯汇编栈测试
pure_asm_stack_test: src/pure_asm_stack_test.c
	$(CC) -O0 -Wall -Wextra -g -o $@ $< $(LDFLAGS)

# 清理
clean:
	rm -f $(TARGETS)
	rm -f *.o *.s
	rm -f perf.data perf.data.old perf_*.data
	rm -f *.out *.log

# 运行所有基准测试
run: all
	@echo "=== 运行栈分配基准测试 ==="
	./stack_bench
	@echo ""
	@echo "=== 运行堆分配基准测试 ==="
	./heap_bench
	@echo ""
	@echo "=== 运行混合对比基准测试 ==="
	./mixed_bench
	@echo ""
	@echo "=== 运行汇编级演示 ==="
	./stack_asm_demo
	@echo ""
	@echo "=== 运行栈增长模式对比测试 ==="
	./stack_growth_comparison

# 使用perf分析
perf: all
	@echo "=== 使用perf分析栈分配 ==="
	perf stat -e cycles,instructions,cache-misses,page-faults ./stack_bench
	@echo ""
	@echo "=== 使用perf分析堆分配 ==="
	perf stat -e cycles,instructions,cache-misses,page-faults ./heap_bench
	@echo ""
	@echo "=== 使用perf分析栈增长模式（验证缺页行为）==="
	perf stat -e page-faults ./stack_growth_comparison

# 使用strace追踪系统调用
strace: all
	@echo "=== strace追踪栈分配系统调用 ==="
	strace -c -e trace=brk,mmap,munmap ./stack_bench
	@echo ""
	@echo "=== strace追踪堆分配系统调用 ==="
	strace -c -e trace=brk,mmap,munmap ./heap_bench

# 详细的strace追踪
strace-verbose: all
	@echo "=== 详细追踪堆分配的brk/mmap调用 ==="
	strace -e trace=brk,mmap,munmap -o heap_syscalls.out ./heap_bench
	@cat heap_syscalls.out

# 生成汇编代码
asm: src/stack_allocation.c src/heap_allocation.c src/stack_asm_demo.c
	@echo "生成汇编代码..."
	$(CC) -S -O2 -fverbose-asm src/stack_allocation.c -o stack_allocation.s
	$(CC) -S -O2 -fverbose-asm src/heap_allocation.c -o heap_allocation.s
	$(CC) -S -O2 -fverbose-asm src/stack_asm_demo.c -o stack_asm_demo.s
	@echo "汇编文件已生成: *.s"

.PHONY: all clean run perf strace strace-verbose asm
