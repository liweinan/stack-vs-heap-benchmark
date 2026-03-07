# 文档索引

## 快速开始

- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - 快速参考卡片
- [README.md](README.md) - 完整项目说明

## 核心问题解答

### 🔍 为什么差距这么大？

**推荐阅读顺序**：

1. **[WHY_SO_DIFFERENT.md](WHY_SO_DIFFERENT.md)** ⭐⭐⭐⭐⭐
   - **最重要的文档！**
   - 完整解答：为什么缺页差距 975 倍？系统调用差距 777 倍？
   - 三大根本原因：VMA 生命周期、页表复用、分配机制
   - 结合内核源码的深度解释

2. **[PERFORMANCE_BREAKDOWN.md](PERFORMANCE_BREAKDOWN.md)** ⭐⭐⭐⭐
   - 性能差异分解分析
   - 逐层剖析：分配/释放、系统调用、缺页、计算
   - 成本金字塔可视化
   - 性能公式推导

3. **[DEEP_ANALYSIS.md](DEEP_ANALYSIS.md)** ⭐⭐⭐⭐
   - 深度技术分析
   - 从分配方式到物理内存管理
   - 批发-零售链条详解
   - 边界情况讨论

4. **[KERNEL_CODE_VERIFICATION.md](KERNEL_CODE_VERIFICATION.md)** ⭐⭐⭐
   - 内核源码验证
   - 具体文件和行号
   - do_mmap, do_munmap, do_anonymous_page
   - expand_stack 实现

## 测试结果

- **[TEST_RESULTS.md](TEST_RESULTS.md)** - 实际测试结果 + 数据分析
- **[EXPECTED_RESULTS.md](EXPECTED_RESULTS.md)** - 预期结果 + 内核源码对照

## 项目信息

- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - 项目总结 + 关键发现
- **[USAGE.md](USAGE.md)** - 详细使用指南

## 可视化总结

- **[VISUAL_SUMMARY.txt](VISUAL_SUMMARY.txt)** - ASCII 艺术图表
- **[FINAL_SUMMARY.txt](FINAL_SUMMARY.txt)** - 最终总结

---

## 按主题索引

### 性能差异分析

```
WHY_SO_DIFFERENT.md
  ├─ 答案 1: 分配成本差异（1.42x）的真相
  │   └─ 真实差距 50 倍（被访问成本稀释）
  ├─ 答案 2: 系统调用差距（777x）
  │   ├─ 栈：VMA 持久（进程级）
  │   └─ 堆：VMA 临时（malloc 级）
  └─ 答案 3: 缺页差距（975x）- 最关键！
      ├─ 栈：页表复用
      └─ 堆：页表重建
```

### 内核机制

```
KERNEL_CODE_VERIFICATION.md
  ├─ 1. do_mmap (mm/mmap.c:337)
  ├─ 2. do_munmap (mm/mmap.c:1067)
  ├─ 3. expand_stack (mm/mmap.c:961)
  ├─ 4. do_anonymous_page (mm/memory.c:5022)
  └─ 5. Buddy 分配器 (mm/page_alloc.c)
```

### 性能分解

```
PERFORMANCE_BREAKDOWN.md
  ├─ 层级 1: 分配/释放成本（200x）
  ├─ 层级 2: 系统调用成本（∞）
  ├─ 层级 3: 缺页异常（975x次数，7x成本）
  └─ 层级 4: 计算成本（8x，缓存局部性）
```

---

## 核心数据

```
┌──────────┬────────┬───────────┬───────┐
│   指标   │   栈   │    堆     │ 差异  │
├──────────┼────────┼───────────┼───────┤
│ 分配成本 │ 50 ns  │ 70 ns     │ 1.42x │
├──────────┼────────┼───────────┼───────┤
│ 系统调用 │ 0 次   │ 2,332 次  │ 777x  │
├──────────┼────────┼───────────┼───────┤
│ 缺页异常 │ 34 次  │ 33,190 次 │ 975x  │
├──────────┼────────┼───────────┼───────┤
│ 总耗时   │ 3.2 ms │ 73.1 ms   │ 23x   │
└──────────┴────────┴───────────┴───────┘
```

## 关键结论

1. **分配/释放是最大差异**（200 倍）
   - 栈：1 条指令（sub sp）
   - 堆：malloc + 系统调用

2. **系统调用差距从无到有**（777 倍）
   - 栈：VMA 持久，0 次系统调用
   - 堆：每次 mmap/munmap

3. **缺页次数差距最大**（975 倍）
   - 栈：页表复用，34 次
   - 堆：页表重建，33,190 次
   - 但实际成本差只有 7 倍（内核优化）

4. **总耗时差距 23 倍**
   - 主要来自分配/释放 + 系统调用
   - 缺页虽然次数差距大，但被优化

---

## 博客论点验证

✅ 栈快是因为分配方式简单 + VMA 持久 + 页表复用
✅ 堆慢是因为 malloc 管理 + 频繁系统调用 + 页表重建
✅ 缺页路径栈与堆等价（单次成本相同，频率不同）
✅ 堆可以快（预分配 + 复用 → 接近栈性能）

**所有测试数据完美验证了博客的理论分析！**

---

## 快速链接

| 我想... | 阅读... |
|---------|---------|
| 快速了解项目 | [QUICK_REFERENCE.md](QUICK_REFERENCE.md) |
| 理解性能差异 | [WHY_SO_DIFFERENT.md](WHY_SO_DIFFERENT.md) |
| 查看测试结果 | [TEST_RESULTS.md](TEST_RESULTS.md) |
| 学习内核实现 | [KERNEL_CODE_VERIFICATION.md](KERNEL_CODE_VERIFICATION.md) |
| 深入技术细节 | [DEEP_ANALYSIS.md](DEEP_ANALYSIS.md) |
| 查看性能分解 | [PERFORMANCE_BREAKDOWN.md](PERFORMANCE_BREAKDOWN.md) |
| 运行测试 | [USAGE.md](USAGE.md) |

---

## 参考资料

- 博客: [栈为什么比堆快](https://weinan.io/2026/03/01/stack-vs-heap-why-stack-faster.html)
- 内核: /Users/weli/works/linux
- 项目: /Users/weli/works/stack-vs-heap-benchmark
