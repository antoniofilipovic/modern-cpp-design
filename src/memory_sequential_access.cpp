// memory_sequential_access.cpp
//
// Demonstrates concepts from:
//   "What Every Programmer Should Know About Memory" — Ulrich Drepper (2007)
//
// ─── SECTION COVERED ──────────────────────────────────────────────────────────
//   § 3.3  Measurement of cache effects (sequential access, working set size)
//
// ─── THE CORE IDEA ────────────────────────────────────────────────────────────
//   Modern CPUs have a memory hierarchy:
//
//      ┌──────┐  ~4 cycles   ┌──────┐  ~12 cycles   ┌──────┐  ~40 cycles   ┌──────┐  ~200 cycles
//      │  L1  │ ──────────▶  │  L2  │ ────────────▶  │  L3  │ ────────────▶  │ RAM  │
//      │ 32KB │              │256KB │                │  8MB │               │  ∞   │
//      └──────┘              └──────┘                └──────┘               └──────┘
//
//   When your "working set" (the data you actively touch) fits entirely in a
//   cache level, every access is served from there.  The moment the working set
//   EXCEEDS a cache level's capacity, lines get evicted and you pay the cost of
//   the next level down.
//
//   Sequential access is the BEST CASE for the hardware because:
//     • The hardware prefetcher recognises the stride-1 pattern and speculatively
//       loads upcoming cache lines BEFORE the CPU actually needs them.
//     • Full cache lines (64 bytes) are loaded, so every byte in a line is used.
//
//   Even so, once the working set doesn't fit in L1 (or L2, or L3) you will see
//   a clear step-down in throughput (GB/s).  This benchmark makes those steps
//   visible.
//
// ─── WHAT TO LOOK FOR IN THE OUTPUT ──────────────────────────────────────────
//   • High and roughly flat bandwidth while the working set is below each cache.
//   • A noticeable drop at the L1→L2, L2→L3, and L3→RAM boundaries.
//   • Even for sequential access the drop at the L3→RAM boundary is dramatic
//     (often 5–10×), because main memory bandwidth is so much lower.
//
// ─── BUILD ────────────────────────────────────────────────────────────────────
//   cmake --build cmake-build-debug --target mem_sequential
//   ./cmake-build-debug/src/mem_sequential
//
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Prevent the compiler from eliminating our reads.
// Without this, an optimising compiler may prove that the loop result is
// unused and delete the entire loop body — giving 0 ns timings.
// ---------------------------------------------------------------------------
static volatile uint64_t global_sink = 0;

// ---------------------------------------------------------------------------
// Try to read a Linux sysfs cache-size file, returning 0 on failure.
// Example path: /sys/devices/system/cpu/cpu0/cache/index1/size
// The file contains strings like "32K" or "8192K".
// ---------------------------------------------------------------------------
static size_t read_cache_size_sysfs(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return 0;
    std::string val;
    f >> val;
    if (val.empty()) return 0;
    size_t multiplier = 1;
    if (val.back() == 'K' || val.back() == 'k') { multiplier = 1024;       val.pop_back(); }
    if (val.back() == 'M' || val.back() == 'm') { multiplier = 1024*1024;  val.pop_back(); }
    try { return std::stoull(val) * multiplier; }
    catch (...) { return 0; }
}

struct CacheSizes {
    size_t l1d = 0;   // L1 data cache
    size_t l2  = 0;
    size_t l3  = 0;
};

// ---------------------------------------------------------------------------
// Detect cache sizes from /sys on Linux.
// Each index under /sys/.../cache/ describes one cache level;
// we look at the "type" file to skip instruction caches.
// ---------------------------------------------------------------------------
static CacheSizes detect_cache_sizes() {
    CacheSizes cs;
    const std::string base = "/sys/devices/system/cpu/cpu0/cache/";

    // Typical layout: index0=L1i or L1d, index1=L1d or L2, index2=L2, index3=L3
    // "type" file contains "Data", "Instruction", or "Unified"
    for (int idx = 0; idx <= 5; ++idx) {
        std::string dir = base + "index" + std::to_string(idx) + "/";
        std::ifstream type_f(dir + "type");
        if (!type_f.is_open()) break;
        std::string type; type_f >> type;
        if (type == "Instruction") continue;

        std::ifstream level_f(dir + "level");
        if (!level_f.is_open()) continue;
        int level = 0; level_f >> level;

        size_t sz = read_cache_size_sysfs(dir + "size");
        if (sz == 0) continue;

        if (level == 1 && cs.l1d == 0) cs.l1d = sz;
        if (level == 2 && cs.l2  == 0) cs.l2  = sz;
        if (level == 3 && cs.l3  == 0) cs.l3  = sz;
    }
    return cs;
}

// ---------------------------------------------------------------------------
// Human-readable byte count.
// ---------------------------------------------------------------------------
static std::string fmt_bytes(size_t bytes) {
    if (bytes >= 1024*1024*1024) return std::to_string(bytes/(1024*1024*1024)) + " GB";
    if (bytes >= 1024*1024)      return std::to_string(bytes/(1024*1024))      + " MB";
    if (bytes >= 1024)           return std::to_string(bytes/1024)             + " KB";
    return std::to_string(bytes) + "  B";
}

// ---------------------------------------------------------------------------
// Label a working-set size relative to the detected cache hierarchy.
// This is the annotation column in the output table.
// ---------------------------------------------------------------------------
static std::string cache_label(size_t ws, const CacheSizes& cs) {
    // We annotate only the first size that crosses each boundary.
    // Caller is responsible for only calling once per transition; here
    // we just return a static description.
    if (cs.l1d && ws <= cs.l1d)                        return "<= L1d";
    if (cs.l2  && ws <= cs.l2  && ws > cs.l1d)         return "<= L2 (exceeds L1d)";
    if (cs.l3  && ws <= cs.l3  && ws > cs.l2)          return "<= L3 (exceeds L2)";
    if (cs.l3  && ws > cs.l3)                           return "> L3  (goes to RAM)";
    // fallback when sysfs unavailable
    return "";
}

// ---------------------------------------------------------------------------
// Core benchmark: read `working_set_bytes` sequentially, repeated enough
// times that we transfer at least `min_total_bytes` in total.
//
// Returns: bandwidth in GB/s.
//
// Key design decisions:
//   1. We use uint64_t loads, 8 bytes at a time — matches a typical register.
//   2. We accumulate into a local checksum and write it to `global_sink` once,
//      so the compiler cannot delete the loop but also doesn't issue a store
//      per iteration (which would pollute bandwidth numbers).
//   3. We run at least two warm-up passes before timing to populate caches
//      and let the hardware prefetcher recognise the pattern.
// ---------------------------------------------------------------------------
static double measure_sequential_read(
    char* buffer,
    size_t working_set_bytes,
    size_t min_total_bytes = 256ULL * 1024 * 1024)   // transfer at least 256 MB
{
    const size_t elem_count = working_set_bytes / sizeof(uint64_t);
    const uint64_t* const begin = reinterpret_cast<uint64_t*>(buffer);
    const uint64_t* const end   = begin + elem_count;

    // Number of full passes needed.
    size_t passes = std::max(size_t(2), min_total_bytes / working_set_bytes);

    // ── Warm-up ──────────────────────────────────────────────────────────────
    // Two warm-up passes: the first faults in all pages (TLB misses); the
    // second lets the prefetcher observe the stride pattern so it is already
    // primed when we start timing.  For large working sets the working-set
    // itself won't fit in cache, which is exactly what we want to measure.
    {
        uint64_t warm = 0;
        for (int w = 0; w < 2; ++w)
            for (const uint64_t* p = begin; p < end; ++p) warm += *p;
        global_sink = warm; // keep the compiler honest
    }

    // ── Timed measurement ────────────────────────────────────────────────────
    uint64_t checksum = 0;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t pass = 0; pass < passes; ++pass) {
        for (const uint64_t* p = begin; p < end; ++p)
            checksum += *p;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    global_sink ^= checksum; // force the reads; XOR to avoid branch on value

    double seconds   = std::chrono::duration<double>(t1 - t0).count();
    double total_read = static_cast<double>(passes) * static_cast<double>(working_set_bytes);
    return total_read / seconds / 1e9; // GB/s
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    CacheSizes cs = detect_cache_sizes();

    std::cout << "\n";
    std::cout << "=== Single-threaded sequential read benchmark ===\n";
    std::cout << "    (What Every Programmer Should Know About Memory — §3.3)\n\n";

    // ── Print detected cache topology ────────────────────────────────────────
    std::cout << "Detected cache sizes (from /sys):\n";
    if (cs.l1d) std::cout << "  L1d : " << fmt_bytes(cs.l1d) << "\n"; else std::cout << "  L1d : unknown\n";
    if (cs.l2)  std::cout << "  L2  : " << fmt_bytes(cs.l2)  << "\n"; else std::cout << "  L2  : unknown\n";
    if (cs.l3)  std::cout << "  L3  : " << fmt_bytes(cs.l3)  << "\n"; else std::cout << "  L3  : unknown\n";
    std::cout << "\n";

    // ── Theory refresher ─────────────────────────────────────────────────────
    std::cout <<
        "Theory (Drepper §3.3):\n"
        "  Sequential access is the BEST CASE for cache performance because:\n"
        "   • The hardware prefetcher detects the stride-1 pattern and loads\n"
        "     upcoming cache lines before the CPU requests them.\n"
        "   • Every byte of each 64-byte cache line is actually consumed\n"
        "     (no wasted bandwidth due to unused words in a line).\n"
        "  Even so, once the working set EXCEEDS a cache level, lines must\n"
        "  be evicted and re-fetched from the next (slower) level.  The\n"
        "  bandwidth drops noticeably at each cache boundary.\n\n";

    // ── Allocate a single large buffer ───────────────────────────────────────
    // We use the maximum working set size and always read from the beginning
    // of this buffer, varying only how far we read each pass.
    // This avoids NUMA effects and keeps virtual-to-physical mappings stable.
    const size_t MAX_WS = 256ULL * 1024 * 1024; // 256 MB
    std::vector<char> buf(MAX_WS);

    // Initialise with non-zero data so the checksum is non-trivial.
    for (size_t i = 0; i < MAX_WS; i += sizeof(uint64_t))
        *reinterpret_cast<uint64_t*>(buf.data() + i) = static_cast<uint64_t>(i) * 6364136223846793005ULL + 1442695040888963407ULL;

    // ── Working set sizes: powers of 2 from 4 KB up to 256 MB ───────────────
    std::vector<size_t> sizes;
    for (size_t s = 4 * 1024; s <= MAX_WS; s *= 2)
        sizes.push_back(s);

    // Column widths
    const int W_SIZE  = 10;
    const int W_BW    = 12;
    const int W_LABEL = 30;

    std::cout << std::left
              << std::setw(W_SIZE)  << "Work.Set"
              << std::setw(W_BW)   << "  BW (GB/s)"
              << std::setw(W_LABEL) << "  Cache level"
              << "\n";
    std::cout << std::string(W_SIZE + W_BW + W_LABEL, '-') << "\n";

    double prev_bw = -1.0;
    for (size_t ws : sizes) {
        double bw = measure_sequential_read(buf.data(), ws);
        std::string label = cache_label(ws, cs);

        // Annotate when bandwidth drops by more than 15% relative to previous
        // measurement — a heuristic sign of crossing a cache boundary.
        std::string drop_flag;
        if (prev_bw > 0.0 && bw < prev_bw * 0.85)
            drop_flag = " <-- bandwidth drop";
        prev_bw = bw;

        std::cout << std::left
                  << std::setw(W_SIZE)  << fmt_bytes(ws)
                  << "  " << std::fixed << std::setprecision(2)
                  << std::setw(W_BW - 2) << bw
                  << "  " << std::setw(W_LABEL) << label
                  << drop_flag
                  << "\n";
    }

    std::cout << "\n";
    std::cout <<
        "Key take-aways:\n"
        "  1. Bandwidth is highest (and flat) while the working set fits in L1d.\n"
        "     Every read hits L1 — latency ~4 cycles, fully pipelined.\n\n"
        "  2. Once the working set exceeds L1d, lines spill to L2.\n"
        "     Bandwidth drops but less than you might expect, because the\n"
        "     hardware prefetcher hides most of the L2 latency.\n\n"
        "  3. Exceeding L2 means refills come from L3.  L3 is shared across\n"
        "     cores and has higher latency, so bandwidth falls further.\n\n"
        "  4. Exceeding L3 is the cliff: every cache miss now goes to DRAM.\n"
        "     DRAM bandwidth is limited by bus width and timing parameters\n"
        "     (tCAS, tRCD, …).  Even with prefetching the drop is dramatic.\n\n"
        "  5. Sequential access is still the BEST possible pattern at every\n"
        "     level.  Random access at the same working set sizes would show\n"
        "     the same staircase, but each step would be far worse because\n"
        "     the prefetcher cannot predict arbitrary addresses.\n"
        "     (See §3.3.2 of the paper for random-access measurements.)\n\n";

    return 0;
}
