# 快速参考指南

## 一键启动

```bash
cd /Users/weli/works/stack-vs-heap-benchmark
./quick-start.sh
```

## 手动使用

### 启动容器
```bash
docker-compose build
docker-compose up -d
docker-compose exec benchmark /bin/bash
```

### 在容器内
```bash
# 编译
make all

# 运行基准测试
make run

# perf 分析
make perf

# strace 追踪
make strace

# 生成汇编
make asm

# 完整测试套件
./scripts/run_all_benchmarks.sh
```

## 核心测试结果（实际运行）

### 性能对比
```
栈分配:  50 ns/次 (基准线)
堆分配:  70 ns/次 (慢 1.42x)
堆复用:  48 ns/次 (接近栈)
```

### 系统调用
```
栈：3 次（仅初始化）
堆：2332 次（1166 mmap + 1164 munmap）
```

### 缺页异常
```
栈：34 次
堆：33,190 次（975 倍差异！）
```

## 验证博客论点

| 论点 | 验证命令 | 结果 |
|------|---------|------|
| 栈无系统调用 | `strace -c ./stack_bench` | ✅ 0 个运行时调用 |
| 堆触发 mmap | `strace -c ./heap_bench` | ✅ 2332 个调用 |
| 缺页频率差异 | `perf stat -e page-faults` | ✅ 975 倍差异 |
| 堆可接近栈 | `./mixed_bench` | ✅ 堆复用 48ns vs 栈 50ns |

## 架构适配

### x86_64 (Intel/AMD)
```asm
sub rsp, N    # 分配栈空间
add rsp, N    # 回收栈空间
```

### ARM64 (Apple Silicon)
```asm
sub sp, sp, #N   # 分配栈空间
add sp, sp, #N   # 回收栈空间
```

代码已自动适配，无需修改！

## 关键文件

```
README.md            # 完整项目说明
USAGE.md             # 详细使用指南
TEST_RESULTS.md      # 实际测试结果
EXPECTED_RESULTS.md  # 预期结果 + 内核对照
PROJECT_SUMMARY.md   # 项目总结
```

## 核心结论

1. **栈比堆快 1.4 倍**（频繁分配场景）
2. **差异来自**：系统调用（0 vs 2330）+ malloc 管理
3. **堆可优化**：预分配 + 复用 → 接近栈
4. **工程启示**：理解成本，选择策略

## 故障排查

### Docker 构建失败
```bash
docker-compose down
docker-compose build --no-cache
```

### perf 不可用
确保 `docker-compose.yml` 中：
```yaml
privileged: true
cap_add: [SYS_ADMIN]
```

### 程序未编译
在容器内：
```bash
make clean && make all
```

## 停止与清理

```bash
# 停止容器
docker-compose down

# 删除镜像
docker-compose down --rmi all

# 清理编译产物
make clean
```

## 扩展实验

1. **修改数组大小**：编辑 `src/mixed_benchmark.c`
2. **测试大块分配**：修改 `LARGE_SIZE`
3. **多线程测试**：添加 pthread
4. **不同分配器**：对比 musl vs glibc

## 参考链接

- 博客：https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html
- 内核：/Users/weli/works/linux
- 项目：/Users/weli/works/stack-vs-heap-benchmark
