# README Badges

可以在 README.md 顶部添加这些 badges：

```markdown
# Stack vs Heap Performance Benchmark

[![CI](https://github.com/liweinan/stack-vs-heap-benchmark/workflows/CI%20-%20Stack%20vs%20Heap%20Benchmark/badge.svg)](https://github.com/liweinan/stack-vs-heap-benchmark/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Docker](https://img.shields.io/badge/docker-Alpine%20Linux-0db7ed.svg)](https://www.docker.com/)
[![Platform](https://img.shields.io/badge/platform-x86__64%20%7C%20ARM64-blue.svg)](https://github.com/liweinan/stack-vs-heap-benchmark)
[![Language](https://img.shields.io/badge/language-C-00599C.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

Performance benchmark demonstrating why stack is faster than heap, with Linux kernel source code analysis.
```

## 可选的额外 badges

### 代码统计
```markdown
[![Lines of Code](https://tokei.rs/b1/github/liweinan/stack-vs-heap-benchmark)](https://github.com/liweinan/stack-vs-heap-benchmark)
```

### 仓库活跃度
```markdown
[![GitHub last commit](https://img.shields.io/github/last-commit/liweinan/stack-vs-heap-benchmark)](https://github.com/liweinan/stack-vs-heap-benchmark/commits/main)
[![GitHub issues](https://img.shields.io/github/issues/liweinan/stack-vs-heap-benchmark)](https://github.com/liweinan/stack-vs-heap-benchmark/issues)
```

### 性能结果徽章（自定义）
```markdown
![Stack Speed](https://img.shields.io/badge/Stack-50%20ns-brightgreen)
![Heap Speed](https://img.shields.io/badge/Heap-70%20ns-orange)
![Speedup](https://img.shields.io/badge/Speedup-23x-red)
```

### 文档
```markdown
[![Documentation](https://img.shields.io/badge/docs-120KB-blue)](https://github.com/liweinan/stack-vs-heap-benchmark/tree/main)
```

## GitHub Topics 建议

在仓库设置中添加以下 topics：

- `performance`
- `benchmark`
- `linux-kernel`
- `memory-management`
- `c`
- `docker`
- `perf`
- `strace`
- `systems-programming`
- `performance-analysis`
- `stack-vs-heap`
- `kernel-analysis`

## GitHub Description 建议

```
Performance benchmark demonstrating why stack is faster than heap,
with comprehensive Linux kernel source code analysis. Includes perf/strace
analysis, cross-platform support (x86_64/ARM64), and 120KB+ documentation.
```

## Social Preview Image

可以创建一个包含关键测试结果的图片作为 social preview：

```
┌──────────────────────────────────────┐
│  Stack vs Heap Benchmark             │
│                                      │
│  Stack:  3.2 ms                      │
│  Heap:   73.1 ms                     │
│  Ratio:  23x                         │
│                                      │
│  Page Faults: 975x difference        │
│  Syscalls: 777x difference           │
└──────────────────────────────────────┘
```

尺寸：1280x640 px（GitHub 推荐）
