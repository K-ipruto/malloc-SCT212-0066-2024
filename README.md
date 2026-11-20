# Custom Heap Memory Allocator (`myalloc`)

**Name:** CALEB KIPRUTO  
**Student ID:** SCT212-0066/2024  

## Build and Run Commands

```bash
 # Build and run the 29-check test suite
make
```

Build and run with AddressSanitizer and UndefinedBehaviorSanitizer
```bash
gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```
Build and run the Task 8 performance benchmark
```bash
make bench
```
Clean compiled binaries
```bash
make clean
```
## Features Implemented

| Task | Description | Status |
| --- | --- | --- |
| 1 | 16-byte aligned block header (`block_t`) and pointer arithmetic helpers | Complete |
| 2 | Heap growth via `sbrk()` in minimum 64 KB chunks with payload alignment padding | Complete |
| 3 | First-fit placement (`find_fit`) and basic `my_malloc()` | Complete |
| 4 | Block splitting when free remainder holds a header plus at least 16 payload bytes | Complete |
| 5 | `my_free()` with invalid pointer detection, double-free guard, and adjacent coalescing | Complete |
| 6 | `my_calloc()` with overflow check and `my_realloc()` with in-place/adjacent expansion | Complete |
| 7 | Best-fit policy (`set_policy`), `heap_stats()`, `heap_dump()`, and `heap_check()` | Complete |
| 8 | Benchmark comparison (`bench.c`) and fragmentation report (`REPORT.md`) | Complete |

## Questions
Question 1: Why must the header size be a multiple of 16 as well as the payload sizes?

Answer:
The header is placed directly before the memory given to the user. Therefore, its size must be a multiple of 16 to ensure that the user's memory starts at the correct position.
The payload size must also be a multiple of 16 so that the next memory block starts at the correct position.
This ensures that all memory blocks are properly aligned and prevents alignment problems.

Question 2: Why can printf() inside my_malloc() cause infinite recursion?

Answer:
printf() may internally use malloc() to allocate memory for its buffers.
If we call printf() inside my_malloc(), it may call malloc() again, which can lead back to our allocator.
This creates an endless loop of function calls, which can eventually cause a stack overflow and crash the program.

Question 3: Your my_free() is O(n). Which extra field would allow you to find the previous block in O(1), and what is this technique called?

Answer:
We can add a prev pointer to the block structure.
This pointer stores the address of the previous block, allowing us to find it directly without searching through the entire linked list.
Another method is to use boundary tags, which store the block's size and allocation information at its end.
This makes it easier to locate the previous block in constant time, O(1).

Question 4: Explain the difference between internal and external fragmentation. Name one place where each occurs in your allocator.

Answer:
Internal fragmentation happens when an allocated memory block has some unused space inside it.
In our allocator, this happens when:
Memory requests are rounded up to multiples of 16.
A block is not split because the remaining space is too small.
External fragmentation happens when free memory is scattered into separate small blocks instead of being available as one large continuous space.
In our allocator, this happens when some blocks are allocated while others are freed, leaving small free spaces between blocks that are still being used.

Question 5: Why does sbrk() memory never get returned to the OS in your allocator? How could you give back memory at the end of the heap?

Answer:
Our allocator does not return memory to the operating system because my_free() only marks a block as free. It does not reduce the program break.
To return memory, the allocator could check whether the last block (tail) is free and whether it reaches the end of the heap.
If it does, the allocator could use a negative sbrk() call to reduce the program break and release that memory back to the operating system.
It would then remove the released block from the linked list.

Question 6: Two blocks are next to each other in your linked list but are not physically adjacent in memory. How did this happen, and what does your coalescing code do about it?

Answer:
This can happen when another library function, such as printf() or the standard malloc(), requests memory between two calls to our allocator's grow() function.
As a result, two blocks may be next to each other in our linked list but have another memory allocation between them.
Our coalesce() function checks whether the blocks are physically next to each other before merging them.
It uses this condition:
```bash
(char *)payload(cur) + cur->size == (char *)cur->next
```
If the addresses do not match, the blocks are not merged.
This prevents our allocator from accidentally combining memory that does not belong to it.

## Known Limitations
Single-threaded only: The allocator is designed for one thread. It does not use mutexes to protect shared variables such as head, tail, and sbrk(). Therefore, simultaneous calls from multiple threads may cause problems.
O(n) searching and coalescing: Both my_malloc() and my_free() may need to go through the entire linked list to find suitable blocks or combine free blocks. This can become slow when there are many blocks.
No heap shrinking: The allocator does not return unused memory at the end of the heap to the operating system. Instead, it keeps the free memory so that it can be reused for future allocations.

## Sources and AI Tool Declaration
Course Materials:
ICS 2305 Systems Programming Practical Assignment Specification 
Provided starter files: myalloc.h, test_myalloc.c, and Makefile.

Reference Documentation:
Linux manual pages: sbrk(2), write(2), and clock_gettime(2).

AI Tools:
Gemini was used to help organize the implementation into smaller steps, review possible problems involving block splitting, coalescing and memory alignment.