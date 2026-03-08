/*
 * Stack Guard Page 演示
 * 展示栈的实际地址、增长方向和 Guard Page 位置
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>

// 获取栈指针（跨平台）
#if defined(__x86_64__) || defined(__amd64__)
    #define GET_SP(sp) __asm__ volatile("mov %%rsp, %0" : "=r"(sp))
#elif defined(__aarch64__) || defined(__arm64__)
    #define GET_SP(sp) __asm__ volatile("mov %0, sp" : "=r"(sp))
#elif defined(__i386__)
    #define GET_SP(sp) __asm__ volatile("mov %%esp, %0" : "=r"(sp))
#else
    #define GET_SP(sp) do { sp = __builtin_frame_address(0); } while(0)
#endif

void print_stack_info(const char *label, int depth) {
    void *sp;
    GET_SP(sp);

    // 在栈上分配一些变量
    char local_var[100];
    snprintf(local_var, sizeof(local_var), "Depth %d", depth);

    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);

    printf("%s:\n", label);
    printf("  Stack pointer (RSP): %p\n", sp);
    printf("  Local variable addr: %p\n", (void *)local_var);
    printf("  Stack limit:         %lu bytes (%.2f MB)\n",
           (unsigned long)limit.rlim_cur,
           (double)limit.rlim_cur / (1024 * 1024));
    printf("\n");
}

void *initial_sp = NULL;

void recursive_stack_walk(int depth, int max_depth) {
    // 分配 1KB 在栈上
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    void *current_sp;
    GET_SP(current_sp);

    // 计算已使用的栈空间
    long stack_used = (char *)initial_sp - (char *)current_sp;

    printf("Depth %4d: SP=%p, Stack used=%ld bytes (%.2f KB)\n",
           depth, current_sp, stack_used, (double)stack_used / 1024);

    if (depth < max_depth) {
        recursive_stack_walk(depth + 1, max_depth);
    }

    // 使用 buffer 防止优化
    buffer[0] = depth & 0xFF;
}

void demonstrate_stack_growth() {
    printf("=== Stack Growth Demonstration ===\n\n");

    GET_SP(initial_sp);
    print_stack_info("Initial state", 0);

    printf("Recursing to allocate stack space (10 levels, 1KB each):\n");
    printf("Stack grows DOWNWARD (addresses decrease)\n\n");

    recursive_stack_walk(0, 10);

    printf("\n");
}

// 演示检测栈边界
void demonstrate_stack_bounds() {
    printf("=== Stack Bounds Detection ===\n\n");

    void *sp;
    GET_SP(sp);

    // 获取栈限制
    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);

    // 估算栈顶（初始位置）
    // 注意：这只是估算，实际栈顶由内核设置
    void *estimated_stack_top = sp;
    void *estimated_stack_bottom = (char *)sp - limit.rlim_cur;
    void *guard_page = (char *)estimated_stack_bottom - getpagesize();

    printf("Current stack pointer:     %p\n", sp);
    printf("Estimated stack top:       %p\n", estimated_stack_top);
    printf("Estimated stack bottom:    %p (- %.2f MB)\n",
           estimated_stack_bottom,
           (double)limit.rlim_cur / (1024 * 1024));
    printf("Estimated guard page:      %p (- %d bytes)\n",
           guard_page, getpagesize());
    printf("\n");

    printf("Memory layout:\n");
    printf("  High address → %p ← Stack top (initial SP)\n", estimated_stack_top);
    printf("                  ↓ Stack grows DOWN\n");
    printf("                  ↓\n");
    printf("                %p ← Current SP\n", sp);
    printf("                  ↓\n");
    printf("                  ↓ (%.2f MB available)\n",
           ((char *)sp - (char *)estimated_stack_bottom) / (1024.0 * 1024.0));
    printf("                  ↓\n");
    printf("  Low address  → %p ← Stack bottom\n", estimated_stack_bottom);
    printf("                 %p ← Guard page (PROT_NONE)\n", guard_page);
    printf("                        ↑ Accessing this causes SIGSEGV!\n");
    printf("\n");
}

// 演示页面大小
void demonstrate_page_size() {
    printf("=== Memory Page Information ===\n\n");

    long page_size = sysconf(_SC_PAGESIZE);
    long phys_pages = sysconf(_SC_PHYS_PAGES);
#ifdef _SC_AVPHYS_PAGES
    long avail_pages = sysconf(_SC_AVPHYS_PAGES);
#else
    long avail_pages = phys_pages / 2;  // macOS doesn't have this
#endif

    printf("Page size:          %ld bytes (%ld KB)\n",
           page_size, page_size / 1024);
    printf("Physical pages:     %ld (%.2f GB)\n",
           phys_pages, (double)(phys_pages * page_size) / (1024 * 1024 * 1024));
    printf("Available pages:    %ld (%.2f GB)\n",
           avail_pages, (double)(avail_pages * page_size) / (1024 * 1024 * 1024));
    printf("\n");

    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);
    long stack_pages = limit.rlim_cur / page_size;

    printf("Stack size:         %lu bytes\n", (unsigned long)limit.rlim_cur);
    printf("Stack pages:        %ld pages (%lu KB)\n",
           stack_pages, (unsigned long)limit.rlim_cur / 1024);
    printf("Guard page:         1 page (%ld bytes)\n", page_size);
    printf("\n");
}

// 演示多线程栈
void *thread_func(void *arg) {
    int thread_id = *(int *)arg;
    void *sp;
    GET_SP(sp);

    printf("Thread %d:\n", thread_id);
    printf("  Current SP:        %p\n", sp);

#ifdef __linux__
    // 获取线程属性（仅 Linux）
    pthread_attr_t attr;
    size_t stack_size;
    void *stack_addr;

    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstacksize(&attr, &stack_size);
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);

    printf("  Stack base:        %p\n", stack_addr);
    printf("  Stack size:        %zu bytes (%.2f MB)\n",
           stack_size, (double)stack_size / (1024 * 1024));
    printf("  Stack top:         %p\n", (char *)stack_addr + stack_size);

    pthread_attr_destroy(&attr);
#else
    printf("  (Thread stack info not available on this platform)\n");
#endif
    printf("\n");
    return NULL;
}

void demonstrate_thread_stacks() {
    printf("=== Thread Stack Demonstration ===\n\n");

    pthread_t threads[3];
    int thread_ids[3] = {1, 2, 3};

    printf("Main thread:\n");
    void *sp;
    GET_SP(sp);
    printf("  Current SP:        %p\n", sp);
    printf("\n");

    // 创建线程
    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    // 等待线程完成
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Note: Each thread has its own stack!\n");
    printf("      Thread stacks are allocated by pthread library\n");
    printf("      Usually smaller than main thread stack\n");
    printf("\n");
}

int main() {
    printf("======================================\n");
    printf("   Stack Memory Layout Demonstration\n");
    printf("======================================\n\n");

    demonstrate_page_size();
    demonstrate_stack_bounds();
    demonstrate_stack_growth();
    demonstrate_thread_stacks();

    printf("=== Summary ===\n\n");
    printf("Key points:\n");
    printf("  1. Stack grows DOWNWARD (from high to low addresses)\n");
    printf("  2. Stack has fixed size limit (~8MB on Linux)\n");
    printf("  3. Guard page below stack catches overflow\n");
    printf("  4. Accessing guard page → Page Fault → SIGSEGV\n");
    printf("  5. Each thread has its own stack\n");
    printf("\n");

    return 0;
}
