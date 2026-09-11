## Chapter 29 Fix A — complete

### Repository state

Initial:

- HEAD: `68f12c017f39752a5747353d053c1d306394eeca`
- Index: clean
- Tracked working tree: clean
- Pre-existing untracked Chapter-29 review directory present

Final:

- HEAD unchanged
- Index clean
- Working tree: `M docs/ARCHITECTURE.md`
- The pre-existing untracked directory disappeared concurrently during the task; I did not access or alter it.

### Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

Modified sections:

- §29.6 Hash-aggregate spill
- §29.7 DISTINCT
- §29.10 Ordered aggregation and streaming DISTINCT

### M29-1 result

Before, baseline hash `PhysicalDistinct` only “may reuse” group-hash infrastructure, leaving its pipeline role indirect.

It now explicitly owns this lifecycle:

```text
Sink
    consume all semantically demanded input
    evaluate DISTINCT values
    retain one state entry per duplicate class
    emit no rows

Finalize
    complete the build
    publish successful readiness

Source
    emit exactly one result row per duplicate class
```

Incomplete or failed state cannot be observed by the Source. First observation of a class does not authorize skipping demanded input. Chapters 20 and 26 remain the demand and early-termination owners.

Ordered/streaming DISTINCT remains a separate capability-conditional alternative.

### N29-5 replacements

- “A later aggregate descriptor may…” → descriptor eligibility based on safe serialization and Combine semantics.
- “every current v1 aggregate” → “every v1 aggregate in §29.3.”
- “A later physical planner may choose…” → present-tense capability-conditional ordered DISTINCT.
- “supports later ordered implementations…” → capability-conditional physical alternatives.
- “Until the runtime capability exists…” → Chapter-38 capability eligibility, enumeration, and validation ownership.

### Ordering and capability handoff

Section 29.10 now states explicitly:

- Chapter 30 owns physical sorting and its complete comparator.
- Chapter 37 owns required/provided `OrderingProperty` proof.
- Chapter 38 owns capability eligibility, costed enumeration, and final-plan validation.
- Admitted ordering must make grouping or DISTINCT equivalence classes contiguous.
- Ordered operators may advertise only properties proven for the selected plan.

### Deferred-policy preservation

No new semantic policy was introduced:

- N29-1 GROUP BY representative: untouched and unresolved.
- N29-2 DISTINCT representative bits: untouched and unresolved.
- D29-1 exact FLOAT64 SUM/AVG policy: unchanged.
- M29-2 ordered-aggregate retained-output ownership: unchanged; the current-group memory wording remains intact.

### Checks

- `git diff --check`: passed
- Diff: **1 file changed, 39 insertions, 7 deletions**
- Staged changes: none
- Verification: not modified
- Implementation: not started
- Build/tests/sanitizers/benchmarks: not run