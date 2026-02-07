<div align="center">

# Programming Assignment 2
## Message-Passing Performance Analysis

**Sayan Das** • MT25041  
M.Tech 1st Year • IIIT Delhi

</div>

---

## Overview

This assignment explores the performance characteristics of three different message-passing implementations in a client-server architecture. The goal is to understand how data copy operations affect throughput, latency, CPU cycles, and cache behavior under various workloads.

The implementation emphasizes code quality with descriptive variable names throughout, making the codebase self-documenting and maintainable. All identifiers use full English words rather than abbreviations, significantly improving readability and reducing cognitive load when navigating the code.

I implemented three approaches with increasing optimization for memory copies:

<table>
<tr>
<td width="33%" valign="top">

### A1: Traditional 2-copy
Uses standard `send()` and `recv()` system calls. Data gets copied twice: once from user space to kernel space during send, and again from kernel space to user space during recv.

**Trade-off:** Simple and portable, but expensive in memory bandwidth.

</td>
<td width="33%" valign="top">

### A2: Shared memory 1-copy
Leverages `mmap()` to create a shared memory region. The client writes directly to shared memory, then uses `send()` for notification only.

**Trade-off:** Halves memory bandwidth, but adds synchronization overhead.

</td>
<td width="33%" valign="top">

### A3: Zero-copy
Uses Linux `splice()` with pipes. Data moves between file descriptors entirely in kernel space without user-space copies.

**Trade-off:** Minimal CPU involvement, but `splice()` overhead hurts small messages.

</td>
</tr>
</table>

---

## Project Structure

```
┌─ Implementation Code
│
├─ MT25041_Part_A1_Client.c          2-copy client (send/recv)
├─ MT25041_Part_A1_Server.c          2-copy server (send/recv)
├─ MT25041_Part_A2_Client.c          1-copy client (mmap + send)
├─ MT25041_Part_A2_Server.c          1-copy server (mmap + recv)
├─ MT25041_Part_A3_Client.c          0-copy client (splice)
├─ MT25041_Part_A3_Server.c          0-copy server (splice)
├─ MT25041_Part_Common.c             Shared utilities
├─ MT25041_Part_Common.h             Common header definitions
└─ Makefile                          Build configuration
│
┌─ Experiment Automation
│
├─ MT25041_Part_C_Config.json        Experiment parameters
├─ MT25041_Part_C_Run_All.sh         Automated test harness
│
┌─ Visualization & Results
│
├─ MT25041_Part_D_Plots_Hardcoded.py Demo plotting (for viva)
├─ MT25041_Part_D_Plots.py           Analysis plotting (reads CSV)
├─ MT25041_Part_B_RawData.csv        Raw experimental data
└─ results/                          Generated plots
```

> **Note:** All source files use production-quality variable naming conventions with descriptive identifiers (e.g., `socket_file_descriptor`, `thread_context`, `message_buffers_array`) instead of cryptic abbreviations. This makes the code self-documenting and significantly easier to understand and maintain.

---

## Implementation Details

### A1: Traditional send/recv (2-copy)

The client allocates a buffer, fills it with data, and calls `send()`. The kernel copies this data into a socket buffer. On the server side, `recv()` copies data from the kernel socket buffer into the server's user-space buffer.

```
┌──────────────┐       ┌───────────────┐       ┌───────────────┐
│ Client Buffer│ ───▶ │ Kernel Socket │ ───▶ │ Server Buffer │
│  (User Space)│       │   (Kernel)    │       │  (User Space) │
└──────────────┘       └───────────────┘       └───────────────┘
     Copy 1                                        Copy 2
```

| Pros | Cons |
|------|------|
| Simple, portable, well-understood | Double memory bandwidth consumption |
| No setup overhead | Potential cache pollution |

### A2: Shared memory with send notification (1-copy)

Both client and server map the same memory region using `shm_open()` and `mmap()`. The client writes directly to this shared region. Instead of sending the actual data, `send()` only transmits a small notification or metadata.

```
┌──────────────────────────────┐
│   Shared Memory Region       │
│  (Accessible by both sides)  │
└──────────────────────────────┘
       ▲              ▲
        │              │
  Client writes   Server reads
        │              │
        └──[ notify ]──┘  ◀── Only metadata copied
```

| Pros | Cons |
|------|------|
| Reduces memory bandwidth by half | Requires coordination |
| Better cache locality | Synchronization overhead |

### A3: Zero-copy splice (0-copy)

The client writes data to a pipe, then uses `splice()` to move data from the pipe to the socket. On receiving, the server splices from the socket to another pipe, then reads. The `splice()` syscall transfers data between file descriptors without copying to user space.

```
┌───────────┐    splice    ┌────────┐    splice    ┌───────────┐
│Client Pipe│ ═══════════▶│ Socket │ ═══════════▶│Server Pipe│
└───────────┘  (in kernel) └────────┘  (in kernel) └───────────┘

All transfers stay in kernel space - zero user-space copies
```

| Pros | Cons |
|------|------|
| Minimal CPU involvement | `splice()` overhead for small messages |
| No user-space buffer needed | Requires pipe setup |

---

## Code Quality and Readability

The entire codebase has been written with emphasis on maintainability and clarity. All variable names are descriptive and self-documenting, making the code easy to understand without extensive comments.

**Variable Naming Convention:**

Instead of cryptic abbreviations, I used full descriptive names throughout:

| Category | Examples |
|----------|----------|
| **Function Parameters** | `argument_count`, `argument_values` (instead of argc, argv) |
| **Socket Descriptors** | `socket_file_descriptor`, `listen_socket_fd` (instead of fd) |
| **Configuration** | `server_configuration`, `client_configuration` (instead of cfg) |
| **Threading** | `thread_context`, `thread_index`, `cpu_pin_base` (instead of ctx, idx, pin_base) |
| **Messaging** | `message_size`, `message_buffers_array`, `total_bytes_sent` (instead of msg_size, msgs, bytes_sent) |
| **Timing** | `operation_start_time_ns`, `round_trip_time_nanoseconds_sum` (instead of start, rtt_ns_sum) |
| **Zero-copy** | `zerocopy_enabled`, `zerocopy_inflight_operations` (instead of zc_enabled, inflight) |

**Benefits:**

- **Self-documenting code**: Variable purpose is clear from its name
- **Reduced cognitive load**: No need to remember what abbreviations mean
- **Easier debugging**: Variable names appear clearly in debuggers and stack traces
- **Better maintainability**: New developers can understand code faster
- **IDE autocomplete**: Longer names with prefixes make autocomplete more useful

For example, compare:
```c
// Old style (cryptic)
ctx->fd, ctx->msg_size, ctx->rtt_ns_sum

// Current implementation (clear)
thread_context->socket_file_descriptor
thread_context->message_size
thread_context->round_trip_time_nanoseconds_sum
```

This naming philosophy extends to all struct types, function names, and local variables throughout the project, making the codebase professional and production-ready.

---

## Quick Start

### Building the Project

```bash
make
```

This generates six executables:

| Implementation | Client | Server |
|----------------|--------|--------|
| **2-copy** | `MT25041_Part_A1_Client` | `MT25041_Part_A1_Server` |
| **1-copy** | `MT25041_Part_A2_Client` | `MT25041_Part_A2_Server` |
| **0-copy** | `MT25041_Part_A3_Client` | `MT25041_Part_A3_Server` |

To clean:
```bash
make clean
```

---

## Running Experiments

### Option 1: Manual Testing (single configuration)

For testing a specific implementation, start the server in one terminal and client in another:

```bash
# Terminal 1: Start server
./MT25041_Part_A1_Server <port> <threads> <msg_size> <duration>

# Example
./MT25041_Part_A1_Server 9090 8 256 3
```

```bash
# Terminal 2: Run client with perf instrumentation
sudo perf stat -e cycles,L1-dcache-load-misses,cache-misses \
    ./MT25041_Part_A1_Client 127.0.0.1 9090 8 256 3
```

**Arguments:**
- `port` — TCP port number
- `threads` — Number of worker threads
- `msg_size` — Message size in bytes
- `duration` — Test duration in seconds

### Option 2: Automated Batch Execution (all 96 experiments)

The assignment requires testing all combinations of parameters. Run the automation script:

```bash
./MT25041_Part_C_Run_All.sh
```

**What it does:**

| Parameter | Values | Count |
|-----------|--------|-------|
| Implementations | A1, A2, A3 | 3 |
| Message sizes | 64, 256, 1024, 4096 bytes | 4 |
| Thread counts | 1, 2, 4, 8 | 4 |
| Modes | Throughput, Latency | 2 |
| **Total experiments** | | **96** |

Each experiment:
1. Starts the appropriate server in background
2. Launches client with perf instrumentation
3. Captures metrics (cycles, L1 misses, LLC misses, context switches)
4. Calculates throughput and latency
5. Appends results to `MT25041_Part_B_RawData.csv`
6. Kills server and moves to next test

**Runtime:** Approximately 8 minutes (optimized from initial 13 minutes by reducing per-test duration from 5s to 3s and warmup from 1s to 0.5s)

---

## Performance Metrics

The assignment specifies collecting both hardware counters and derived metrics:

### Hardware Counters (via perf stat)

| Metric | Event Name | Description |
|--------|------------|-------------|
| **CPU Cycles** | `cycles` | Total CPU cycles consumed during the test. Indicates raw computational cost. |
| **L1 Cache Misses** | `L1-dcache-load-misses` | L1 data cache load misses. When data isn't in L1, the CPU must fetch from L2/L3, adding latency. |
| **LLC Misses** | `cache-misses` | Last-level cache misses. Expensive because they require main memory access. |
| **Context Switches** | `context-switches` | Number of times the OS scheduler switched threads. High values indicate contention or blocking I/O. |

> **Note:** I initially tried L3-specific events (`l3_cache_accesses`, `l3_misses`) but they caused perf to hang on my AMD system. The standard `cache-misses` event works reliably and represents LLC behavior across architectures.

### Derived Metrics

| Metric | Formula | Unit | Meaning |
|--------|---------|------|---------|
| **Throughput** | `(total_bytes × 8) / (duration_s × 10⁹)` | Gbps | Data transfer rate |
| **Latency** | `(duration_s × 10⁶) / (total_bytes / msg_size)` | μs | Average round-trip time per message |
| **Cycles per Byte** | `total_cycles / total_bytes` | cycles/byte | CPU efficiency (lower is better) |

---

## Configuration

The file `MT25041_Part_C_Config.json` controls experimental parameters:

```json
{
  "msg_sizes": [64, 256, 1024, 4096],
  "thread_counts": [1, 2, 4, 8],
  "duration_s": 3,
  "warmup_s": 0.5
}
```

**Customization options:**

| Parameter | Purpose | Example Values |
|-----------|---------|----------------|
| `msg_sizes` | Message sizes to test (bytes) | `[64, 256, 1024, 4096, 8192]` |
| `thread_counts` | Thread counts to test | `[1, 2, 4, 8, 16, 32]` |
| `duration_s` | Test duration per experiment | `3` (seconds) |
| `warmup_s` | Warmup period before measurement | `0.5` (seconds) |

You can modify these to test different scenarios, such as larger message sizes (8KB, 16KB) or different thread counts for many-core systems.

---

## Generating Plots

> **Important:** The assignment explicitly requires that plotting code uses hardcoded values, not CSV parsing, for demonstration during viva.

### For Demo/Viva (Assignment Requirement)

```bash
python3 MT25041_Part_D_Plots_Hardcoded.py results/
```

This script contains hardcoded arrays extracted from my experimental runs and generates:

| Plot File | Description |
|-----------|-------------|
| `MT25041_Part_D_Throughput_vs_MsgSize.png` | Throughput vs Message Size (at 8 threads) |
| `MT25041_Part_D_Latency_vs_Threads.png` | Latency vs Thread Count (at 64-byte messages) |
| `MT25041_Part_D_CacheMisses_vs_MsgSize.png` | L1 and LLC Cache Misses vs Message Size (at 8 threads) |
| `MT25041_Part_D_CyclesPerByte_vs_MsgSize.png` | CPU Cycles per Byte vs Message Size (at 8 threads) |

The hardcoded data represents actual measurements from my system. I manually transcribed representative values from the CSV after verifying they were stable across multiple runs.

### For Analysis (Not for Demo)

```bash
python3 MT25041_Part_D_Plots.py MT25041_Part_B_RawData.csv results/
```

I also wrote this version that reads the CSV for my own analysis. It makes regenerating plots easier after re-running experiments. But per assignment requirements, this should **NOT** be used during demonstration.

**Plotting Features:**
- Matplotlib with modern Seaborn aesthetics
- Iosevka Nerd Font (falls back to system fonts)
- Proper legends, grid lines, and axis labels
- System information annotation (CPU model, kernel version)
- 300 DPI PNG output

---

## Experimental Results

Running on my AMD system with Linux kernel, here's what I observed:

### Throughput Analysis (8 threads)

#### Small Messages (64 bytes)

| Implementation | Throughput | Relative Performance |
|----------------|------------|----------------------|
| **2-copy** | 0.81 Gbps | 🟢 Best |
| **1-copy** | 0.77 Gbps | 🟡 95% |
| **0-copy** | 0.002 Gbps | 🔴 0.2% |

The 0-copy implementation performs terribly with small messages. The overhead of `splice()` and pipe operations completely dominates any benefit from avoiding copies. For tiny messages, the traditional approach wins.

#### Large Messages (4KB)

| Implementation | Throughput | Relative Performance |
|----------------|------------|----------------------|
| **2-copy** | 40.1 Gbps | 🟢 Best |
| **1-copy** | 36.1 Gbps | 🟡 90% |
| **0-copy** | 26.2 Gbps | 🟠 65% |

Even at larger sizes, the 2-copy approach leads in throughput on my system. I suspect this is because `send`/`recv` are highly optimized in the kernel, and the loopback interface handles in-kernel copies very efficiently. The 1-copy approach does well but has some overhead from shared memory synchronization.

### Latency Analysis (64-byte messages)

| Threads | 2-copy | 1-copy | 0-copy | Winner |
|---------|--------|--------|--------|--------|
| **1** | 14.36 μs | 14.26 μs | 16.19 μs | 1-copy |
| **8** | 17.31 μs | 17.63 μs | 20.05 μs | 2-copy |

The 0-copy approach consistently has higher latency, which makes sense given the syscall overhead. Interestingly, latency increases with more threads due to contention and context switching.

### Cache Behavior (4KB messages, 8 threads)

| Implementation | LLC Misses | Cache Efficiency |
|----------------|------------|------------------|
| **0-copy** | 297M | 🟢 Best |
| **1-copy** | 362M | 🟡 +22% |
| **2-copy** | 451M | 🔴 +52% |

Here the 0-copy approach shines. By keeping data in kernel buffers, it avoids polluting the user-space cache. The 2-copy approach has the highest LLC miss rate because data gets touched multiple times in different contexts.

### Key Insight

> **There's no universal winner.** Small-message workloads favor the simple 2-copy approach due to its low syscall overhead. Large-message throughput also favors 2-copy on my system, likely due to kernel optimizations. However, 0-copy has the best cache efficiency, which could matter in cache-sensitive applications or when running alongside other workloads. The 1-copy approach sits in the middle, offering moderate improvement in cache usage without the syscall overhead of `splice`.

---

## System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Linux with perf_events support (kernel 2.6.31+) |
| **Privileges** | Root access (sudo) required for perf stat |
| **Compiler** | GCC with C11 support |
| **Libraries** | pthread, rt (for mmap/shm_* functions) |
| **Python** | 3.6+ with matplotlib and numpy |
| **Hardware** | Multi-core CPU, sufficient RAM for concurrent threads |

---

## Known Issues & Solutions

### L3-specific perf events hang on some AMD processors

**Problem:** I initially tried using `l3_cache_accesses` and `l3_misses` events directly, but perf hung indefinitely on my AMD Ryzen system.

**Solution:** Switched to the generic `cache-misses` event, which is more portable and represents LLC behavior across different architectures. This is a known issue with AMD performance monitoring units where some L3 events aren't properly exposed.

### Socket buffer limits with large messages and many threads

**Problem:** When running with 4KB messages and 8 threads, some `send()` calls can block if the socket buffer fills up.

**Solution:** Increased `SO_SNDBUF` and `SO_RCVBUF` to 256KB to mitigate this. For even larger configurations, you might need to tune these further or use non-blocking I/O.

### splice() initialization overhead

**Problem:** The 0-copy implementation has a one-time setup cost for creating pipes.

**Solution:** Included a warmup period (0.5 seconds) before starting measurements. Without warmup, the first few measurements would be skewed.

---

## Assignment Compliance

This project follows all assignment specifications:

| Requirement | Status | Implementation |
|-------------|--------|----------------|
| File naming convention | ✓ | All files prefixed with `MT25041_Part_X` |
| Roll number in Makefile | ✓ | Comment at the top of Makefile |
| Hardcoded plotting script | ✓ | `MT25041_Part_D_Plots_Hardcoded.py` |
| Four required plots | ✓ | All generated with proper labels and legends |
| Performance metrics | ✓ | Collected via perf stat as specified |
| Three implementations | ✓ | 2-copy, 1-copy, 0-copy with distinct semantics |
| Automated experiment runner | ✓ | Bash script for reproducibility |
| CSV format for raw data | ✓ | `MT25041_Part_B_RawData.csv` |
| Comprehensive README | ✓ | This document |
| Code quality | ✓ | Descriptive variable names, self-documenting code |

---

## Design Rationale

### Why these message sizes?

| Size | Rationale |
|------|-----------|
| **64 bytes** | Typical small message, cache-line friendly |
| **256 bytes** | Small-medium message |
| **1024 bytes** | 1KB boundary |
| **4096 bytes** | Page size on most systems |

This covers the spectrum from cache-line scale to page scale, revealing different performance characteristics at each level.

### Why these thread counts?

**1, 2, 4, 8** represents a doubling progression, showing scaling behavior. Most development machines have at least 4 cores, and 8 threads tests hyperthreading or higher core counts.

### Why throughput vs latency modes?

**Throughput mode** emphasizes continuous data streaming with less frequent synchronization.  
**Latency mode** focuses on request-response patterns with tighter synchronization.

Real applications might lean one way or the other, so testing both gives a complete picture.

---

<div align="center">

**Sayan Das**  
Roll Number: MT25041  
M.Tech 1st Year, IIIT Delhi  
February 2026

</div>
