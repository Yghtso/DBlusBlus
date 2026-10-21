# Appendix C. Capability Boundaries

This appendix locates capability boundaries in their owning chapters and preserves
additional excluded capabilities that have no separate subsystem contract. Detailed
owners determine whether a mechanism is required, optional, or an implementation choice.
Implementation availability and authorization gates are recorded in `PROJECT_STATE.md`.

## C.1 Subsystem scope owners

| Subject | Canonical scope owner |
|---|---|
| Single-node purpose, distributed-system non-goals, primary storage model, SQL compatibility, extensions, JIT and GPU execution | §§1.1–1.4 |
| Supported platform and optional operating-system experiments | §3.1 |
| Page-file organization, allocation, truncation, extent and reuse boundaries | §§4.7, 4.11, 6.12, 7.4.1; segmented WAL is separately defined by §12.2 |
| Inline tuple storage, HOT-like updates, physical scalar/string responsibility, schema translation | §§4.6.3, 5.6, 5.11–5.13, 8.23 |
| Flat FSM persistence and advisory candidate selection | §§6.2–6.10 |
| I/O strategy, buffer representation, and replacement | §§7.2, 7.8, 7.12 |
| B+ key components, compression/overflow, latch coupling, reverse scans, and heap visibility | §§8.3, 8.6, 8.19–8.23 |
| Isolation, snapshot representation, conflict-key families, DDL gates and immediate uniqueness | §§9.5, 9.7–9.8, 11.1, 11.10–11.13 |
| Synchronous commit, retention, no ordinary user-DML physical undo, and statement retry | §§12.15, 13.10, 13.16, 15.7 |
| RID reuse, read epochs, vacuum coordination and scheduling | §§14.5–14.17 |
| Namespace, logical scalar types, grammar, parameters, subqueries, SQL exclusions and offline index build | §§16.2, 17.2–17.10, 18.7, 18.10–18.12, 19.19, 20.14, 21.8, 21.19 |
| Physical operator availability, runtime vector vocabulary, borrowed data and temporary spill | §§22.4.1, 23.3, 23.12, 24.7, 31.10 |
| Optional MergeJoin, ordered aggregation, external sorting and parallel execution | §§28.12, 29.10, 30.1–30.5, 32.1–32.13 |
| Optimizer framework boundary and absence of ordinary adaptive/runtime reoptimization | §§33.1–33.2, 33.7 |
| ANALYZE scheduling, full-scan collection, persisted statistics and single-column estimates | §§34.2, 34.7, 34.9, 35.17 |
| One-index access paths, uncorrelated-subquery planning, and runtime statistics feedback | §§36.16, 37.17, 38.22 |

These references do not exclude capabilities that their owners define. In particular,
external sorting, safe physical RID reuse, parallel heap-scan semantics and scheduler
ownership are defined contracts. Their implementation prerequisites do not change their
architectural scope.

## C.2 Additional excluded capabilities

The following capabilities are outside the defined architecture. Introducing them as
supported capabilities requires an architecture revision:

- segmentation of ordinary random-access page files;
- page and tuple compression;
- a visibility map;
- B-link internal right-links and alternative versioned, latch-free or lock-free B+
  traversal that replaces the canonical page-latch protocol; §8.19.3's optimistic root
  validation remains part of that protocol;
- specialized B+ bulk loading beyond ordinary MTR-based offline index construction;
- covering/include-column index payloads;
- parallel index scans and ordered B+ range partitioning without an owning source protocol;
- clustered-storage or index-locality redesign;
- a doublewrite buffer;
- two-phase commit, XA, distributed transactions, and prepared transactions;
- lock escalation and speculative unique-insertion optimizations;
- commit-sequence-number or timestamp-oracle visibility schemes;
- online backup and read-only follower snapshots;
- pervasive radix hash-join/partitioning redesign; the existing Grace and parallel hash
  partitioning contracts remain with their owners;
- distributed exchange operators and remote spilling;
- engine-wide late-materialization redesign;
- Bloom-filter pushdown and general Bloom-filter/semi-join reduction planning; the exact
  capability-gated uncorrelated-subquery rewrites in §§20.14.11 and 37.17 remain distinct;
- arbitrary memo transformation rules over expression groups;
- learned cardinality estimation;
- DPccp/connected-subgraph enumeration as a replacement for the defined join search;
- materialized-view matching;
- join elimination beyond the explicitly proven rewrite rules;
- partition pruning;
- generic/custom parameter-sensitive prepared-plan policy;
- detailed probabilistic parallel-memory/cost modeling;
- distributed/GPU optimizer cost models.

## C.3 Optional mechanisms and implementation freedom

Optionality and implementation freedom do not make the associated invariants optional.
Use the detailed owner for applicability, correctness and resource requirements:

| Mechanism | Owner and permitted scope |
|---|---|
| Background vacuum scheduling | §14.17 permits scheduling around the required explicit/manual path; the mutation coordinator boundary remains §32.13 |
| Maintenance-triggered ANALYZE | §34.2 distinguishes optional scheduling from explicit ANALYZE |
| MergeJoin and ordered aggregation | §§22.4.1, 28.12, 29.10 define capability-conditional implementations |
| Comparator-equivalent radix/normalized/hybrid sorting | §30.4 permits strategies preserving the full comparator |
| NUMA, architecture-specific SIMD and safe prefetch | §§32.10–32.12 define optional policies; CPU prefetch does not redefine §7.2 storage I/O |
| Huge pages and alternative platform I/O experiments | §3.1 permits experiments subject to the owning persistence and durability contracts |
| FSM runtime candidate representation and tie-breaking | §6.9 leaves these choices to the implementation |
| BufferPool map/coordination structures | §7.8 fixes frame/publication invariants rather than one runtime map |
| LockManager structures and synchronization | §11.12 fixes conflict and wakeup semantics rather than one container |
| Transaction registration/capture synchronization | §9.8 owns atomic registration/capture; the snapshot active-set representation remains §9.7.2 |
| WAL append-buffer structures and synchronization | §12.12.5 preserves reservation/publication invariants without selecting one implementation |
| Work stealing and scheduler tuning | §32.8 permits optimizations within the required dependency and ownership model |

Development prerequisites for these mechanisms belong in
[`DEVELOPMENT.md`](../DEVELOPMENT.md#capability-integration-prerequisites).
