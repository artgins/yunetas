# Performance Analysis - Final Report (Focused & Actionable)

**Date**: 2025-12-29
**Exclusions**:
- ✅ strlen() issues (already fixed)
- ✅ json_equal() issues (already fixed)
- ✅ FSM lookups (known, optimized with most-used-first strategy)
- ✅ MQTT broker (in progress)

**Focus**: Novel findings in Timeranger2 and core framework

---

## Context Understood

1. **Timeranger2 Architecture**:
   - Time-series key/value database on flat files
   - **Append-only** record files (sequential writes ✅)
   - Few updates in index files
   - Good: Sequential I/O is optimal for this design

2. **Event Loop**:
   - Already uses **io_uring** ✅ (excellent choice!)
   - Zero-copy capabilities available
   - Modern async I/O

3. **Core Optimizations Done**:
   - strlen caching
   - json_equal for comparisons
   - FSM state ordering

---

## Remaining Performance Issues (Novel Findings)

### 1. Timeranger2: Cache `tranger2_topic_size()` Result

**Current**: Recalculates total rows every time

**Location**: `kernel/c/timeranger2/src/timeranger2.c:1154-1174`

```c
PUBLIC uint64_t tranger2_topic_size(...) {  // WARNING fn slow for thousands of keys!
    uint64_t total = 0;
    json_t *topic_cache = json_object_get(topic, "cache");
    void *iter = json_object_iter(topic_cache);
    while(iter) {
        const char *key = json_object_iter_key(iter);
        total += get_topic_key_rows(gobj, topic, key);  // 4-level nested lookup
        iter = json_object_iter_next(topic_cache, iter);
    }
    return total;
}
```

**Issue**: O(n) with 4-level JSON nesting per key

**Fix**: Cache in topic metadata, update on append/delete

```c
// In topic JSON
{
    "total_rows": 0,  // Cached count
    "cache": { ... }
}

// On append: increment atomically
// On query: return cached value O(1)
```

**Impact**: O(n×4 lookups) → O(1)
**Effort**: 30-60 minutes
**Risk**: Low (just add counter)

---

### 2. Timeranger2: Deep Nested Lookups in Hot Path

**Location**: `kernel/c/timeranger2/src/timeranger2.c:7403-7417`

```c
PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key)
{
    json_t *jn_rows = json_object_get(
        json_object_get(
            json_object_get(
                json_object_get(topic, "cache"),
                key),
            "total"),
        "rows");

    return json_integer_value(jn_rows);
}
```

**Issue**:
- 4 separate hash table lookups
- Called frequently (including in the loop above)
- Poor cache locality

**Alternative**: Use helper function with path string

```c
// Use kw_get_int with path notation (likely already exists)
char path[256];
snprintf(path, sizeof(path), "cache`%s`total`rows", key);
return kw_get_int(gobj, topic, path, 0, 0);
```

Or even better, **cache per-key totals** in simpler structure:

```c
// In topic metadata
{
    "key_totals": {
        "key1": 1234,
        "key2": 5678
    }
}

// Then just: json_integer_value(json_object_get(key_totals, key))
```

**Impact**: Faster lookups, better cache locality
**Effort**: 1-2 hours
**Risk**: Low

---

### 3. Functions Marked as "Slow" - Profiling Needed

The following functions are explicitly marked as slow but may not have been profiled:

| Function | Location | Mark | Action Needed |
|----------|----------|------|---------------|
| `get_ordered_filename_array()` | helpers.c:3189 | "WARNING too slow for thousands of files" | Profile actual usage |
| `json_range_list()` | helpers.c:2054 | "WARNING slow for large ranges" | Audit usage patterns |
| `json_listsrange2set()` | helpers.c:2083 | "WARNING TOO SLOW" | Check if used in hot paths |
| `kw_find_path_()` | kwid.c:392 | "WARNING too slow" | Avoid in hot paths |

**Recommendation**:
1. Profile to confirm these are actually bottlenecks
2. If used in hot paths, optimize or replace
3. If not in hot paths, document when NOT to use them

**Effort**: 2-4 hours profiling
**Impact**: TBD based on profiling

---

### 4. Timeranger2: File Operations Without Error Retry

**Observation**: All file operations fail immediately on error

Example at `timeranger2.c:2483`:
```c
size_t ln = write(content_fp, p, md_record.__size__);
if(ln != md_record.__size__) {
    // Log error and return -1 immediately
    return -1;
}
```

**Issue**: No retry logic for transient failures
- EINTR (interrupted syscall)
- EAGAIN (would block)
- Disk full scenarios

**Recommendation**: Add retry wrapper for I/O operations

```c
ssize_t retry_write(int fd, const void *buf, size_t count) {
    ssize_t result;
    do {
        result = write(fd, buf, count);
    } while (result < 0 && errno == EINTR);

    return result;
}
```

**Impact**: More robust I/O
**Effort**: 2-3 hours
**Risk**: Low

---

### 5. Potential: Batch Index Updates

**Context**: You mentioned "few updates in index files"

**Question**: Are index updates batched or per-record?

If index is updated on **every record append**:
```c
append_record() {
    write_to_record_file();
    update_index_file();  // <-- Every time?
}
```

**Potential Optimization**: Batch index updates

```c
append_record() {
    write_to_record_file();
    mark_index_dirty();
}

// Periodically or on explicit flush:
flush_index_updates() {
    batch_update_index();
}
```

**Trade-off**:
- ✅ Faster appends (fewer index I/O)
- ❌ Index may be slightly stale (acceptable for time-series?)

**Question for You**: How critical is index freshness?

---

### 6. JSON Reference Counting Overhead

**Observation**: Heavy use of JSON with reference counting

Example patterns throughout:
```c
json_incref(value);    // Atomic increment
json_decref(value);    // Atomic decrement (may trigger free)
```

For high-frequency operations, reference counting has overhead:
- Atomic operations (cache line bouncing in multi-thread)
- Deallocation checks on every decref

**Potential Optimization**: For known-lifetime objects, skip ref counting

```c
// Instead of:
json_incref(kw);
process(kw);
json_decref(kw);

// If lifetime is clear:
process(kw);  // Caller manages lifetime
```

**Note**: Only applicable where ownership is clear

**Impact**: Modest (5-10% in JSON-heavy paths)
**Effort**: Architectural (case-by-case basis)
**Risk**: Medium (must be careful with ownership)

---

## Summary: Truly Actionable Items

### **Quick Wins (< 1 hour)**

1. ✅ **Cache `tranger2_topic_size()` in metadata**
   - Add `total_rows` field
   - Increment on append
   - O(n) → O(1)

### **Medium Effort (1-4 hours)**

2. **Optimize `get_topic_key_rows()` deep nesting**
   - Cache key totals in flatter structure
   - Or use path notation

3. **Add I/O retry logic**
   - Wrapper for write/read to handle EINTR
   - More robust

### **Needs Investigation (2-8 hours)**

4. **Profile "slow" functions**
   - Confirm they're actually bottlenecks
   - Optimize if in hot paths

5. **Index update strategy**
   - Are updates batched?
   - Can they be batched more?

### **Long-Term (Weeks)**

6. **JSON ownership optimization**
   - Reduce ref counting where safe
   - Requires careful analysis

---

## Positive Findings (Already Optimal)

✅ **Event loop uses io_uring** - Modern, high-performance async I/O
✅ **Append-only record files** - Optimal for time-series
✅ **Sequential writes** - Good for disk I/O
✅ **FSM optimization strategy** - Most-used-first is smart for small collections
✅ **String/JSON fixes done** - Major bottlenecks already addressed

---

## Questions to Guide Further Optimization

To prioritize remaining work:

1. **What's your target write throughput?** (records/sec per topic)
2. **How many keys per topic typically?** (affects topic_size calculation)
3. **How often is `tranger2_topic_size()` called?** (frequency determines priority)
4. **Index update frequency**: Per-record or batched?
5. **Query patterns**: More reads or writes? (optimize accordingly)
6. **Latency vs throughput**: What matters more?

---

## Final Recommendation

**Start with**: Cache `tranger2_topic_size()` - trivial fix, clear benefit

**Then profile**: See if other "slow" functions actually matter in your workload

**Consider**: Index batching if applicable to your consistency requirements

---

**Report Focus**: Actionable items only, respecting your architecture and existing optimizations.
