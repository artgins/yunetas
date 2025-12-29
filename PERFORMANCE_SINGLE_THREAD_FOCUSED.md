# Performance Analysis - Single-Threaded Architecture Focus

**Date**: 2025-12-29
**Architecture**: Single-threaded binaries by design
**Event Loop**: io_uring (async I/O in single thread)

---

## Single-Threaded Implications

### ✅ What This Eliminates

1. **No lock contention** - No mutex bottlenecks
2. **No atomic operations overhead** - json_incref/decref are simple increments
3. **No cache line bouncing** - Single CPU cache
4. **No thread synchronization** - Simpler reasoning
5. **No race conditions** - Deterministic execution

### 🎯 What Matters for Single-Thread Performance

1. **Algorithmic complexity** - O(n) vs O(1) matters most
2. **Cache locality** - Data structure layout matters
3. **Memory allocation patterns** - Heap fragmentation
4. **I/O efficiency** - Already excellent with io_uring
5. **Hot path optimizations** - Every cycle counts in main loop

---

## Revised Priority: Single-Thread Optimizations

### **Priority 1: Algorithmic Complexity (Biggest Impact)**

#### 1.1 Cache `tranger2_topic_size()` - O(n) → O(1)

**Location**: `kernel/c/timeranger2/src/timeranger2.c:1154`

**Current**: Iterates all keys, 4-level nested lookup each
```c
while(iter) {
    const char *key = json_object_iter_key(iter);
    total += get_topic_key_rows(gobj, topic, key);  // O(n) with nested lookups
    iter = json_object_iter_next(topic_cache, iter);
}
```

**Fix**: Maintain counter
```c
// In topic metadata:
{
    "total_rows": 12345  // Updated on append/delete
}

// Query becomes O(1):
return json_integer_value(json_object_get(topic, "total_rows"));
```

**Impact**: **O(n×4) → O(1)** - Critical for large datasets
**Effort**: 30 minutes
**Single-thread benefit**: Eliminates main loop iterations

---

#### 1.2 FSM Lookups - Already Optimized ✅

You mentioned: "most used strings (pointer) in first position"

This is **perfect for single-threaded**:
- Linear search through small array (< 20 items)
- Most common case = first position = 1 comparison
- CPU branch predictor will learn the pattern
- No cache coherency overhead
- Likely faster than hash table for N < 10-20

**No action needed** - Current strategy is optimal for single-thread.

---

### **Priority 2: Memory Allocation (Single-Thread Focus)**

In single-threaded architecture, allocations are expensive because:
- Heap management overhead
- Cache misses
- No parallelism to hide latency

#### 2.1 Reduce Allocation Frequency

**Current patterns** (from earlier analysis):
```c
// Multiple small allocations
char *t1 = gbmem_strdup(token1);
char *t2 = gbmem_strdup(token2);
// ... many more
```

**Single-thread optimization**: Arena allocator

```c
// Per-request arena (stack or thread-local)
struct arena {
    char buffer[64*1024];
    size_t used;
};

// Fast bump allocation (no malloc!)
void* arena_alloc(arena_t *a, size_t size) {
    void *ptr = a->buffer + a->used;
    a->used += size;
    return ptr;
}

// Reset at end of request
arena_reset(&arena);  // Just set used = 0
```

**Benefits for single-thread**:
- No malloc/free overhead
- Perfect cache locality (sequential memory)
- No heap fragmentation
- Extremely fast (pointer bump)

**Use cases**:
- Topic tokenization (MQTT - but you said skip for now)
- Temporary JSON processing
- String building

**Impact**: 10-100x faster allocation
**Effort**: 1-2 weeks (infrastructure)

---

#### 2.2 JSON Object Pooling

For frequently created/destroyed JSON objects:

```c
// Pre-allocate pool at startup
json_t *json_pool[1000];
int pool_size = 0;

// Get from pool (single-thread safe)
json_t *json_pool_get() {
    if(pool_size > 0) {
        return json_pool[--pool_size];  // O(1)
    }
    return json_object();  // Fallback to allocation
}

// Return to pool
void json_pool_put(json_t *obj) {
    json_object_clear(obj);
    if(pool_size < 1000) {
        json_pool[pool_size++] = obj;
    } else {
        json_decref(obj);
    }
}
```

**Single-thread advantage**: No locking needed!

**Impact**: Reduces malloc/free by 90%
**Effort**: 4-8 hours

---

### **Priority 3: Cache Locality (Single-Thread Critical)**

With single thread, CPU cache is your friend:

#### 3.1 Flatten Deep JSON Nesting

**Current**: `timeranger2.c:7403`
```c
json_t *jn_rows = json_object_get(
    json_object_get(
        json_object_get(
            json_object_get(topic, "cache"),
            key),
        "total"),
    "rows");
```

**Problem**: 4 separate hash lookups = 4 cache misses

**Single-thread fix**: Cache frequently accessed values

```c
// In topic structure, cache hot paths:
typedef struct {
    json_t *json_topic;      // Full JSON
    json_t *cache;           // Cached pointer to cache object
    json_t *key_totals;      // Cached pointer to totals
    // ... other hot fields
} topic_cache_t;

// Access becomes:
json_t *rows = json_object_get(
    json_object_get(tc->key_totals, key),
    "rows"
);  // Only 2 lookups instead of 4!
```

**Impact**: Better cache locality, fewer pointer chases
**Effort**: 2-4 hours

---

### **Priority 4: I/O Patterns (Already Optimal) ✅**

**Current**: io_uring with single-threaded event loop

This is **ideal** for single-thread:
- Async I/O without threads
- Batch syscalls
- Zero-copy where possible
- Perfect for event-driven architecture

**Append-only writes** are optimal:
- Sequential writes (fast)
- No seeks in record file
- Minimal index updates

**No changes needed** - architecture is sound.

---

## Single-Thread Anti-Patterns to Avoid

❌ **Don't use**: Thread pools, parallel algorithms
❌ **Don't worry about**: Atomics, locks, thread-safety
✅ **Do focus on**: Tight loops, cache locality, allocations
✅ **Do leverage**: Predictable single-threaded execution

---

## Profiling Strategy for Single-Thread

### Tools that Matter:

1. **perf record -e cache-misses**
   - Find cache-inefficient code
   - Single-thread = all about cache

2. **perf record -e cpu-cycles**
   - Find hot paths
   - Where is CPU time spent?

3. **heaptrack**
   - Memory allocation patterns
   - Heap fragmentation

4. **Flamegraphs**
   - Visualize call stacks
   - Easy to spot hot paths

### What to Look For:

- **CPU-bound bottlenecks** (no parallelism to hide them)
- **Allocation hotspots** (malloc shows up prominently)
- **Cache misses** (expensive with no other threads to do work)
- **Long-running functions** (block entire event loop)

---

## Recommended Optimizations (Revised for Single-Thread)

### **Quick Wins** (< 1 day)

1. ✅ **Cache `tranger2_topic_size()`**
   - O(n) → O(1)
   - 30 minutes

2. ✅ **Flatten `get_topic_key_rows()` nesting**
   - Cache intermediate JSON pointers
   - 2-4 hours

### **Medium Effort** (1 week)

3. **JSON object pooling**
   - Eliminate allocation overhead
   - Single-thread makes this trivial (no locking)
   - 4-8 hours

4. **Profile with perf**
   - Find real bottlenecks in your workload
   - 4 hours

### **Long-Term** (2-4 weeks)

5. **Arena allocator for hot paths**
   - Temporary allocations
   - Perfect for single-thread
   - 1-2 weeks

---

## Questions for Optimization Priority

Given single-threaded architecture:

1. **CPU utilization**: Are you CPU-bound or I/O-bound?
   - If I/O-bound: Focus on batching, async patterns (already good)
   - If CPU-bound: Focus on algorithms, cache locality

2. **Memory pressure**: High allocation rate?
   - Measure with `heaptrack`
   - Object pooling pays off more if high

3. **Event loop latency**: Any operations blocking the loop?
   - Single-thread = any blocking operation stalls everything
   - Ensure all I/O is truly async

4. **Workload characteristics**:
   - Small records, high frequency? → Focus on allocations
   - Large records, low frequency? → Focus on I/O
   - Many queries? → Focus on caching

---

## Architecture Assessment: Single-Thread is Excellent ✅

**Your architecture is well-suited for single-thread**:

1. ✅ **io_uring** - Perfect for async I/O without threads
2. ✅ **Event-driven** - Natural fit for single-thread
3. ✅ **Append-only** - Sequential writes (no contention)
4. ✅ **Time-series** - Predictable access patterns

**What single-thread gives you**:

- Simpler reasoning (no race conditions)
- Better cache utilization (one CPU core)
- Lower memory overhead (no thread stacks)
- Deterministic execution (easier debugging)

**Trade-offs**:

- Can't utilize multiple cores for CPU-bound work
- One slow operation blocks everything
- Must ensure all I/O is async

---

## Final Recommendations (Single-Thread Aware)

### **Do This Week**:

1. Cache `tranger2_topic_size()` (30 min, clear win)
2. Run `perf record ./yuneta_binary` to find real hotspots

### **Do This Month**:

3. Implement JSON object pooling (single-thread makes this easy)
4. Flatten hot-path JSON access patterns

### **Consider Long-Term**:

5. Arena allocator for temporary allocations
6. Profile-guided optimization based on real workload

---

## Summary: Single-Thread Changes Everything

**What matters less now**:
- ~~Lock contention~~ (none)
- ~~Atomic overhead~~ (none)
- ~~Thread synchronization~~ (none)

**What matters more**:
- ✅ Algorithmic complexity (can't hide with parallelism)
- ✅ Cache locality (one CPU, one cache)
- ✅ Memory allocations (very visible)
- ✅ Event loop latency (any blocking = bad)

**Your architecture is sound** - io_uring + single-thread + event-driven is a proven pattern (Node.js, Redis, nginx).

The optimizations should focus on **hot paths, caching, and allocations** rather than concurrency concerns.

---

**End of Report**
