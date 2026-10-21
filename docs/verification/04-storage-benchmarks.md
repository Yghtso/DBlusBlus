# Storage Benchmarks

Establish measurements before optimizing storage mechanisms.

Measure at least:

```text
sequential page read throughput
sequential page write throughput
buffer-pool hit lookup
buffer-pool miss + read
heap insert throughput
heap sequential scan throughput
tuple encode throughput
tuple decode throughput
```

Run benchmarks using:

```text
warm cache
cold-ish cache where practical
tiny buffer pool
large buffer pool
```

Apply the shared Chapter-42 benchmark evidence policy. For each fixture, predeclare a
replayable page/file population (or generator and seed), page count and access order,
measured operation count and bytes, target regime, and actual configured BufferPool frame
capacity. Record the working-set extent in pages and its relation to that capacity;
distinguish BufferPool residency from OS/file-cache warmth and CPU-cache behavior. Use
owner observations for BufferPool hits/misses and completed physical page reads/writes
(§40.1), separately from harness-counted logical work. A successful API call or wrapper
summary alone does not prove a physical transfer or the intended hit/miss path.

- Sequential page reads use a fixed valid page extent in sequential order, a known content
  oracle, an exact logical page/byte denominator, and a declared BufferPool/file-cache
  precondition. Report completed physical reads separately from logical fetches.
- Sequential page writes use a fixed extent and deterministic valid page contents, an
  exact page/byte denominator, and completed-write owner observations. Validate resulting
  contents through the storage owner outside the timed interval, or declare and apply a
  consistent validation window. This fixture does not redefine WAL, synchronization, or
  durability semantics.
- A resident-hit fixture uses a working set that fits its BufferPool, preconditions the
  target pages into resident state, and confirms measured hit-path fetches with canonical
  BufferPool hit observations (and absence of a corresponding physical read where that
  inference is valid). Prior access alone is not residency proof. A miss-plus-read fixture
  instead establishes target nonresidency at lookup, for example with fresh BufferPool
  state or controlled page selection, and confirms both a completed miss claim and a
  completed physical read. Do not rely on probabilistic eviction or a cold-sounding label.
- The tiny-BufferPool fixture uses an owner-valid capacity below its accessed working set
  and confirms meaningful miss/replacement pressure with owner observations. The large
  fixture provides capacity for its intended resident working set and confirms residency
  and hits. For deliberately less-warm file-cache cases, record the deconditioning method
  and observed initial state; where the method is unavailable, report that limitation
  under the shared applicability policy rather than claiming a perfectly cold OS cache.
- Heap insertion replays a known tuple stream with declared row count, fixed/variable
  width shape, payload generator/seed, and BufferPool configuration; count successful
  inserted rows in the declared interval. Heap sequential scan starts from a known
  page/row population, declares width and residency regime, and checks returned rows or
  cardinality against an independent expected result. Reuse heap/tuple correctness owners;
  failed or skipped rows do not contribute valid throughput.
- Tuple encode/decode replays nontrivial deterministic fixed-width, nullable, and
  variable-width values under the physical tuple/type owners. Declare tuple count,
  encoded byte count, measured interval, and throughput unit (tuples/sec, bytes/sec, or
  both), retaining row shape so unlike widths are not silently compared as equal work.

Do not optimize based only on intuition.
