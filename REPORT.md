# Task 8 Benchmark Report — Custom Heap Allocator (`myalloc`) 

## 1. Benchmark Methodology

The benchmark program (`bench.c`) executes an identical, deterministic sequence of **50,000 random allocation and free operations** (payload sizes uniformly distributed between `1` and `4096` bytes across `1024` active slots, seeded with `srand(42)`) under three configurations:

1. **`myalloc` (`FIT_FIRST`)**
2. **`myalloc` (`FIT_BEST`)**
3. **Standard C Library (`glibc` `malloc`/`free`)**

Each configuration runs inside its own `fork()`ed child process so that every policy starts with a clean, unfragmented initial process break. Execution time is measured using `clock_gettime(CLOCK_MONOTONIC, ...)`, and heap metrics are collected immediately after the 50,000 operations via `heap_stats()`.

## 2. Experimental Measurements

| Allocator / Policy | Runtime (ms) | `heap_bytes` | `free_bytes` | `free_blocks` | `largest_free` | External Fragmentation (`1 - largest_free / free_bytes`) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`myalloc` (`FIT_FIRST`)** | 245.106 ms | 1,376,256 | 204,624 | 223 | 50,944 | 0.7510 (75.10%) |
| **`myalloc` (`FIT_BEST`)** | 292.675 ms | 1,310,720 | 140,400 | 150 | 53,280 | 0.6205 (62.05%) |
| **`glibc` (`malloc`/`free`)** | 3.909 ms | N/A | N/A | N/A | N/A | N/A |

## 3. Policy Selection & Comparison with `glibc`

For this workload, I would choose **`FIT_BEST`**. Although `FIT_BEST` took 47.57 ms longer than `FIT_FIRST` (292.675 ms vs. 245.106 ms) because it scans the entire block list on every allocation rather than stopping at the first match, it packed holes tightly enough to avoid an entire 64 KB `sbrk()` heap expansion (`1,310,720` bytes vs. `1,376,256` bytes), reduced fragmented free holes by 32.7% (`150` vs. `223` free blocks), and lowered external fragmentation from `0.7510` to `0.6205`[cite: 23].

`myalloc` is substantially slower and less space-efficient than `glibc`'s `ptmalloc2` allocator because `myalloc` maintains a single linear linked list of both allocated and free blocks—requiring $O(n)$ list traversals on every `my_malloc()` and full-list scans on every `my_free()`—and attaches a 32-byte header to every block[cite: 23]. In contrast, `glibc` uses segregated size-class bins (`tcache`, fastbins, small/large bins) for $O(1)$ allocation/free, boundary tags for $O(1)$ coalescing, and compact 8–16 byte headers with no next-pointer overhead on allocated chunks.
