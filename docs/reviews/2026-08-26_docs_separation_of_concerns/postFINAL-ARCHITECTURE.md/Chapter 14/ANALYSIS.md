# 1. Verdict

**CHAPTER 14 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 14 is substantially detailed and internally disciplined, but three unresolved cross-section safety edges permit architecture-forbidden outcomes:

1. A write-capable transaction can remain active while the status cutoff advances past its TxnId, then introduce a new persistent `xmin`/`xmax` dependency below the cutoff.
2. Cutoff publication does not explicitly require the freeze/normalization WAL needed after a crash to be durable before `database.control` publishes the new cutoff.
3. Physical RID reuse does not account for `TUPLE_WRITE(TableId,RID)` holders/waiters that retain the RID while read-epoch protection is intentionally absent during waits.

These are frozen semantic questions; this review did not repair them.

| Severity | Count |
|---|---:|
| BLOCKING | 3 |
| MAJOR | 0 |
| MINOR | 4 |
| EDITORIAL | 0 |

Primary scope: [Chapter 14](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:10592), through the line before [Chapter 15](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11600).

Context consulted:

- Architecture front matter
- Chapters 4–13 at the requested reclamation boundaries
- Chapter 15 DML integration
- Chapter 21 object retirement
- Chapter 34 statistics/ANALYZE ownership
- §39 failure semantics
- §41 verification obligations
- `docs/DEVELOPMENT.md`
- `docs/VERIFICATION.md`
- `docs/PROJECT_STATE.md`

No source or review artifacts were inspected.

---

# 2. Actual Chapter-14 heading inventory

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 14 | Vacuum and Storage Reclamation | Chapter-wide reclamation contract | Architecture-appropriate |
| 14.1 | Scope | Vacuum ownership boundary | Architecture with terminology issue |
| 14.2 | Global snapshot horizon | Logical MVCC reclamation horizon | Architecture-appropriate |
| 14.3 | Tuple-version garbage eligibility | Candidate classification | Architecture-appropriate |
| 14.3.1 | Aborted creator | Aborted-insert garbage | Architecture-appropriate |
| 14.3.2 | Globally dead committed version | Committed-delete/update death predicate | Architecture-appropriate |
| 14.4 | In-progress transaction metadata | Nonterminal skip/no-wait rule | Architecture-appropriate |
| 14.5 | Two-phase physical RID reclamation | `NORMAL → DEAD → UNUSED` protocol | Architecture with semantic gap |
| 14.6 | ReadEpochManager | Physical RID identity protection | Architecture with semantic gap |
| 14.6.1 | Reader registration | Epoch entry/exit | Architecture-appropriate |
| 14.6.2 | RID retirement | Retirement linearization and exhaustion | Architecture-appropriate |
| 14.6.3 | Exact grace predicate | Exact reuse barrier | Architecture-appropriate |
| 14.6.4 | Crash/restart | Epoch reset and DEAD re-enqueue | Architecture-appropriate |
| 14.7 | Why MVCC visibility is insufficient for RID reuse | Physical-identity rationale | Architecture-appropriate |
| 14.8 | Persistent DEAD state and crash behavior | Restart interpretation of DEAD | Architecture-appropriate |
| 14.9 | Vacuum index-cleanup protocol | Exact multi-index cleanup before DEAD | Architecture-appropriate |
| 14.10 | Version-chain splicing | `prev`-RID dependency removal | Architecture-appropriate |
| 14.11 | Revalidation under concurrent activity | Heap candidate revalidation | Architecture-appropriate |
| 14.12 | DEAD to UNUSED and free-slot publication | Canonical reusable slot publication | Architecture-appropriate |
| 14.13 | Metadata normalization and freezing | Status-dependency removal | Architecture-appropriate |
| 14.13.1 | Aborted xmax | Aborted deleter normalization | Architecture-appropriate |
| 14.13.2 | Frozen committed creator | Creator freezing | Architecture-appropriate |
| 14.14 | Transaction-status retention and physical reclamation | Status-independence/cutoff contract | Architecture with semantic gap |
| 14.14.1 | Page-aligned reclaim cutoff | Exact cutoff mapping | Architecture-appropriate |
| 14.14.2 | Safe reclaim order | Cutoff/control/page-reclamation order | Architecture with semantic and role issues |
| 14.14.3 | Lookup below the cutoff | `RETIRED` behavior | Architecture-appropriate |
| 14.15 | B+ garbage cleanup | Exact stale-entry deletion | Architecture-appropriate |
| 14.16 | FSM and maintenance statistics | Advisory updates/counters | Architecture with roadmap leakage |
| 14.17 | Vacuum execution baseline | Manual baseline and scheduling scope | Architecture with roadmap leakage |
| 14.17.1 | Maintenance coordination | Cross-maintenance ownership and concurrency | Architecture with semantic gaps |
| — | Operation classes and ownership domains | Maintenance operation scopes | Architecture-appropriate |
| — | Object lifetime and publication gates | LIVE/RETIRING ownership | Architecture-appropriate |
| — | Table VACUUM ownership, foreground work, and RID reuse | Table-local vacuum serialization | Architecture with semantic gap |
| — | ANALYZE snapshot, immutable identity, and publication revalidation | ANALYZE/DROP linearization | Architecture-appropriate |
| — | Concurrent statistics publication, cache, and garbage collection | StatsVersion/cache ordering | Architecture-appropriate |
| — | Status-history guard and cutoff publication | Guard/reclaimer protocol | Architecture with semantic gaps |
| — | Compatibility matrix | Maintenance concurrency outcomes | Architecture-appropriate |
| — | Lifecycle, failure, and lock ordering | READY/shutdown/failure contract | Architecture with semantic gap |
| — | Required race outcomes | Canonical race results | Architecture-appropriate |
| — | Forbidden maintenance implementations | Explicit safety prohibitions | Architecture-appropriate |
| 14.18 | Vacuum/reclamation invariants | Consolidated invariants | Architecture-appropriate |

---

# 3. Section-by-section review

Legend: `C` clean/clear, `N` clean with note, `F` finding, `—` not locally owned.

| Section | Timeless | Ownership | Depth | Terminology | Snapshot/epoch | DEAD/reuse | Freeze | Status/cutoff | Retention | RID/index | Crash | Failure | Xrefs | Semantics | Status |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 14 | C | C | C | C | C | C | C | C | C | C | C | C | C | F | FINDING |
| 14.1 | C | C | C | F | — | C | C | C | C | C | — | — | C | C | FINDING |
| 14.2 | C | C | C | C | C | — | — | — | — | — | — | — | C | C | CLEAN |
| 14.3 | C | C | C | C | C | C | — | — | — | — | — | C | C | C | CLEAN |
| 14.3.1 | C | C | C | C | C | C | — | — | — | — | — | C | C | C | CLEAN |
| 14.3.2 | C | C | N | C | C | C | — | — | — | — | — | C | C | C | CLEAN WITH NOTE |
| 14.4 | C | C | C | C | C | C | — | — | — | — | — | C | C | C | CLEAN |
| 14.5 | C | C | C | C | C | F | — | — | — | F | C | — | C | F | FINDING |
| 14.6 | C | C | C | C | C | F | — | — | — | F | C | C | C | F | FINDING |
| 14.6.1 | C | C | C | C | C | C | — | — | — | C | — | — | C | C | CLEAN |
| 14.6.2 | C | C | C | C | C | C | — | — | — | C | — | C | C | C | CLEAN |
| 14.6.3 | C | C | C | C | C | C | — | — | — | C | — | — | C | C | CLEAN |
| 14.6.4 | C | C | C | C | C | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.7 | C | C | C | C | C | C | — | — | — | C | — | — | C | C | CLEAN |
| 14.8 | C | C | C | C | — | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.9 | C | C | C | C | C | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.10 | C | C | C | C | — | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.11 | C | C | C | C | — | C | — | — | — | C | — | C | C | C | CLEAN |
| 14.12 | C | C | C | C | C | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.13 | C | C | C | C | C | — | C | C | C | — | C | C | C | C | CLEAN |
| 14.13.1 | C | C | C | C | C | — | C | C | C | — | C | C | C | C | CLEAN |
| 14.13.2 | C | C | N | C | C | — | C | C | C | — | C | C | C | C | CLEAN WITH NOTE |
| 14.14 | C | C | C | C | C | — | C | F | F | — | F | C | C | F | FINDING |
| 14.14.1 | C | C | C | C | — | — | — | C | C | — | C | C | C | C | CLEAN |
| 14.14.2 | C | F | C | C | C | — | C | F | F | — | F | C | C | F | FINDING |
| 14.14.3 | C | C | C | C | — | — | C | C | C | — | C | C | C | C | CLEAN |
| 14.15 | C | C | C | C | C | C | — | — | — | C | C | C | C | C | CLEAN |
| 14.16 | F | F | C | C | — | C | — | — | — | — | — | C | C | C | FINDING |
| 14.17 | F | F | C | C | — | — | — | — | — | — | — | C | C | C | FINDING |
| 14.17.1 | C | C | C | C | C | F | C | F | F | F | F | C | C | F | FINDING |
| Operation classes | C | C | C | C | C | C | C | C | C | C | C | C | C | F | FINDING |
| Object gates | C | C | C | C | — | C | — | — | — | C | C | C | C | C | CLEAN |
| VACUUM ownership | C | C | C | C | C | F | — | — | — | F | C | C | C | F | FINDING |
| ANALYZE publication | C | C | C | C | C | — | — | — | — | C | C | C | C | C | CLEAN |
| Statistics publication | C | C | C | C | C | — | — | C | C | — | C | C | C | C | CLEAN |
| Status-history guard | C | C | C | C | C | — | C | F | F | — | F | C | C | F | FINDING |
| Compatibility matrix | C | C | C | C | C | C | C | F | C | F | C | C | C | F | FINDING |
| Lifecycle/failure | C | C | C | C | C | F | C | F | F | F | F | C | C | F | FINDING |
| Required races | C | C | C | C | C | C | C | C | C | C | C | C | C | F | FINDING |
| Forbidden implementations | C | C | C | C | C | C | C | C | C | F | C | C | N | F | FINDING |
| 14.18 | C | C | C | C | C | F | C | F | F | F | C | C | C | F | FINDING |

The `F` semantic entries trace to the three cross-section frozen questions rather than independent defects in every affected subsection.

---

# 4. Ownership-boundary assessment

| Mechanism | Canonical owner | Lifetime | Persistent/runtime | Protects | Reclamation consequence |
|---|---|---|---|---|---|
| SQL snapshot | Chapters 9/14.2 | Statement attempt under RC; transaction under RR | Runtime | Logical visibility | Delays logical death horizon |
| MVCC visibility | Chapter 10 | Tuple evaluation | Runtime over persistent metadata | Visible/invisible/error result | Does not authorize physical reuse |
| `TUPLE_WRITE` | Chapter 11 | Through terminal publication | Runtime | Writer conflict identity | Missing from RID-reuse proof |
| Page latch | Chapters 7/8 | Short operation | Runtime | Page bytes/structure | Does not protect later identity reuse |
| BufferPool pin | Chapter 7 | Guard lifetime | Runtime | Frame residency | Does not by itself preserve logical RID identity |
| Read epoch | Chapter 14.6 | While RID may be retained/dereferenced | Runtime | Physical RID identity | Blocks `DEAD → UNUSED` |
| `StatusHistoryGuard` | §14.17.1 | Status-dependent vacuum pass/recheck | Runtime | Status range at/above guard cutoff | Clamps cutoff |
| Checkpoint/DPT `rec_lsn` | Chapters 12–13 | Dirty interval | Runtime metadata backed by WAL | Crash reconstruction | Blocks WAL recycling |
| Status cutoff | §§13.2, 14.14 | Monotonic database lifetime | Persistent plus runtime mirror | Retired status range | Allows `RETIRED` and sparse page removal |
| Vacuum/reclamation | Chapter 14 | Maintenance pass/publication unit | Mixed | Dependency elimination and reuse | Converts logical history into physical reuse |

The chapter correctly separates SQL visibility, status retirement, and physical RID reuse. The missing edges concern admission of new dependencies and lock-key identity, not conflation of those three proofs.

---

# 5. Snapshot, epoch, and protection assessments

## Snapshot versus read epoch

| Property | SQL snapshot | Read epoch |
|---|---|---|
| Question answered | Is a version logically visible? | May a physical RID change identity? |
| Owner | Chapters 9–10 and §14.2 | §14.6 |
| RC lifetime | One statement attempt | As long as execution retains/dereferences RIDs |
| RR lifetime | Transaction snapshot | Physical RID-use lifetime, not necessarily whole transaction |
| Persistent | No | No |
| Blocks | Global logical death | `DEAD → UNUSED` and whole-page RID reuse |
| Survives crash | No | No |

Read-epoch entry is required before index consumption or otherwise retaining a stored RID beyond the current protected page operation. Exit decrements the exact registered epoch; no RID may remain dereferenceable after protection ends unless another protocol independently makes it safe.

Nested guards are not given special reentrant semantics. Multiple registrations are representable through per-epoch counts; one guard never changes epoch while alive.

Epoch domain:

- `uint64`
- initial `1`
- `0` invalid
- `UINT64_MAX` may be current but cannot be assigned as a retirement/increment
- restart or proven complete quiescence may reinitialize
- no wrap.

The single mutex registry is an intentional v1 specialization. It is not a source-layout dependency, though it deliberately constrains implementation freedom.

## Guard/protection comparison

| Protection | Protects | Lifetime | Blocks | Substitute for another? |
|---|---|---|---|---|
| SQL snapshot | MVCC logical history | Statement/transaction | Logical death horizon | No |
| Page latch | Current page bytes | Short critical section | Concurrent byte mutation | No |
| BufferPool pin | Frame residency | Guard | Eviction/frame reuse | No |
| `TUPLE_WRITE` | Logical writer target RID | Transaction terminal boundary | Competing writers | Not currently integrated with RID reclamation |
| Read epoch | RID physical identity | RID-use interval | Slot/page RID reuse | No |
| `StatusHistoryGuard` | Status range | Vacuum scan/recheck | Cutoff advance | No |

---

# 6. Horizon, DEAD, normalization, and freezing

## Horizon

`global_oldest_snapshot_xmin` is the minimum registered snapshot `xmin`; if none exists, it is `next_txn_id`.

- RC between statements contributes no snapshot.
- RR retains its transaction snapshot.
- An active transaction without a snapshot does not constrain the logical horizon merely by existing.
- A long snapshot may block reclamation indefinitely.
- A long read epoch separately blocks physical reuse.

## DEAD predicate and treatment

A tuple is a garbage candidate when:

- its creator is ABORTED; or
- creator is committed/frozen, effective deleter is COMMITTED, and `xmax` precedes the global horizon.

`DEAD` publication additionally means:

- the version is semantically unnecessary;
- every required exact secondary-index entry is absent;
- the RID remains nonreusable.

Aborted inserts and globally old committed delete/update versions follow this same physical cleanup protocol. `DEAD` is not `UNUSED`.

## Freezing/normalization matrix

| Input status | Freeze xmin? | Normalize xmax? | DEAD? | Error/defer | Dependency afterward |
|---|---:|---:|---:|---|---|
| Old COMMITTED xmin | yes, after horizon proof | — | no | — | Creator independent |
| FROZEN xmin | already frozen | — | no | — | Creator independent |
| ABORTED xmin | no | — | garbage candidate | cleanup protocol | Removed once physical state retired |
| IN_PROGRESS xmin | no | no | no | defer | Creator status retained |
| RETIRED but required xmin | no | no | no | invariant/corruption | Invalid state |
| ABORTED xmax | — | yes: invalid/zero cmax | no | — | Deleter independent |
| COMMITTED old xmax | — | no | garbage candidate | remove or retain status | Dependency remains while live metadata remains |
| IN_PROGRESS xmax | — | no | no | defer | Deleter status retained |
| INVALID xmax sentinel | — | no | no | valid no-deleter | Independent |
| RESERVED/invalid status result | no | no | no | fail | No guessing |
| Lookup I/O/corruption | no | no | no | propagate | No mutation |

Freezing and normalization are WAL/page-LSN mutations. Freezing preserves visibility because the creator has already become committed-visible to every legal future snapshot. A frozen creator does not make an ordinary `xmax` independent.

---

# 7. Status-dependency and cutoff assessments

## Status-dependency table

| Persistent state | Creator status needed? | Deleter status needed? | Status-independent? | Required before cutoff passes | Physical RID still needed? |
|---|---:|---:|---:|---|---:|
| Live tuple, normal committed xmin | yes | field-dependent | no | Freeze or remove tuple | yes |
| FROZEN creator | no | field-dependent | only creator side | Resolve xmax independently | yes |
| Aborted creator NORMAL | yes until classified | usually irrelevant | no | Physical/semantic removal | until reuse proof |
| Live tuple, committed normal xmax | creator-dependent | yes | no | Remove tuple or canonical rewrite if one exists | yes |
| Live tuple, aborted xmax | creator-dependent | yes before normalization | no | Normalize xmax | yes |
| Normalized aborted xmax | creator-dependent | no | deleter side yes | Resolve/freeze creator | yes |
| Physically removed tuple metadata | no | no | yes | No further status action | possibly, until RID-reference proof |
| DEAD tuple | no ordinary visibility lookup | no ordinary visibility lookup | semantically status-independent | Preserve RID barriers | yes until UNUSED |
| Persistent `prev` RID | no transaction outcome itself | no | status-independent | Splice before RID reuse | yes |
| Stale index RID | indirect heap dependency | indirect | status may be independent | Remove before RID reuse | yes |
| Catalog MVCC tuple | same as heap | same as heap | field-dependent | Freeze/normalize/remove | yes |
| StatsVersion TxnId payload | no | no | yes, opaque identity | None | no |

## Cutoff definition

| Quantity | Type | Alignment | Meaning | Minimum | Maximum | Monotonic | Owner |
|---|---|---|---|---:|---:|---:|---|
| `txn_status_reclaim_before` | uint64 | `2 + n×32640` | Every normal TxnId below it is retired | 2 | 18,446,744,073,708,474,242 | yes | `database.control` |
| Read-epoch counter | uint64 runtime | none | RID retirement generation | 1 | `UINT64_MAX` current, not retireable | restart/quiescent reseed only | §14.6 |

For the cutoff maximum:

```text
MAX_RESERVED_TXN_ID_END = 18,446,744,073,708,503,042
max aligned cutoff      = 18,446,744,073,708,474,242
remaining slack         = 28,800
last retired TxnId      = 18,446,744,073,708,474,241
```

The cutoff is exclusive: `txn_id < cutoff` returns `RETIRED`.

`RETIRED` never means COMMITTED or ABORTED. It means the original outcome must no longer be needed by a valid persistent correctness object.

## Status-page reclamation

| Page relation to cutoff | Reclaim? | Representation | Lookup | Page numbers change? | Recovery |
|---|---:|---|---|---:|---|
| Wholly at/above cutoff | no | Ordinary page | Read/validate status | no | Reconstruct normally |
| Wholly below cutoff | yes | Optional sparse hole | `RETIRED`, no page read | no | Do not recreate |
| Boundary/misaligned | no legal published cutoff | N/A | Reject control state | no | Open fails |
| Interior retired page | yes | Keep-size hole | `RETIRED` below cutoff | no | Hole legal |
| Trailing retired page | yes | Hole, not logical truncation | `RETIRED` | no | File length preserved |

Status pages are never renumbered. `PageNo 0` remains the superblock.

---

# 8. Cutoff publication and retention

## Current publication stages

| Stage | Crash-safe? | Reopen cutoff | Status pages still required? | Physical deletion legal? |
|---|---:|---|---:|---:|
| Dependencies remain | yes, old state | old | yes | no |
| Freeze/normalize only in memory | no basis for new cutoff | old | yes | no |
| WAL appended but not known durable | no basis explicitly stated | old/new race is underdefined | yes | no |
| Required WAL durable, pages dirty, WAL retained | conceptually safe | old until control sync | yes under old cutoff | no |
| New control slot pending/torn | yes if old objects retained | old unless new slot validates | yes | no |
| New control cutoff durable | intended safe | new | no below cutoff | yes after runtime publication/drain |
| Runtime cutoff published | yes | new | no new below-cutoff pin | frame retirement may start |
| Frames/DPT retired | yes | new | no | hole punch allowed |
| Hole punch complete/fails | yes | new | no | complete or retry |

The architecture clearly orders durable cutoff before page deletion and preserves old-control safety for normal torn publication. The unresolved edge is the missing explicit durability requirement between the prerequisite tuple-page WAL and control publication.

## Checkpoint/WAL-retention matrix

| Dependency | Earliest retained LSN | Publication dependency | Deletion allowed? | Crash safety |
|---|---|---|---:|---|
| Dirty frozen heap page | Dirty interval `rec_lsn` | Must remain reconstructible | no before dependency clears | safe if WAL is durable/retained |
| Dirty normalized heap page | Dirty interval `rec_lsn` | Same | no | same |
| Dirty nonretired TXN_STATUS page | Preparatory `F=rec_lsn` | DPT/checkpoint | no | complete image plus terminals |
| TXN_STATUS page below durable cutoff | none after legal retirement | Cutoff authoritative | yes | recovery skips history |
| Checkpoint with old cutoff | Existing control remains authoritative | Preserve required pages/WAL | conditional | safe |
| Checkpoint with new cutoff | Must agree with selected control and DPT filtering | New control authoritative | yes below cutoff | safe if prerequisite WAL durability is defined |
| WAL segment candidate | All checkpoint/DPT/status dependencies clear | Retention floor | yes | directory durability required |

A forced checkpoint after every ordinary vacuum mutation is not required. Status cutoff publication needs a stronger explicit durability edge than ordinary dirty-page retention alone.

---

# 9. RID reuse and read-epoch assessments

## RID-reuse table

| State | Remove indexes? | Mark DEAD? | Set UNUSED? | Reuse RID? | Reason |
|---|---:|---:|---:|---:|---|
| Merely invisible to worker | no authority | no | no | no | Local visibility is insufficient |
| Globally garbage eligible | yes | after all exact removals | no | no | Logical proof only |
| Some index references remain | continue cleanup | no | no | no | Stale candidate could alias |
| All indexes absent | complete DEAD publication | yes | no | no | Epoch/prev barriers remain |
| Persistent DEAD, old epoch active | already absent | already | no | no | Reader may hold RID |
| DEAD, `prev` dependency remains | already absent | already | no | no | Surviving link could alias |
| DEAD, `TUPLE_WRITE` holder/waiter exists | architecture unresolved | already/possible | underdefined | **must not** | Missing lock-key barrier |
| All persistent/index/epoch/lock references cleared | already absent | yes | yes | yes | Canonical reuse point |
| UNUSED on free list | no stale references | no | already | yes | Reusable slot |

## Read-epoch usage

| Operation | Epoch required? | Acquisition | Release | Blocks reuse? |
|---|---:|---|---|---:|
| Heap scan retaining RID beyond page operation | yes | before retention | after final RID use | yes |
| Index scan | yes | before consuming entry | after heap/recheck use | yes |
| RID batch/materialization | yes | before materialization | after all dereferences | yes |
| Heap fetch from protected candidate | yes | before candidate can outlive latch | after fetch/recheck | yes |
| UNIQUE candidate | yes | before consuming RID | drop before wait; fresh reprobe after wake | yes |
| UPDATE/DELETE candidate | protection required while RID is dereferenced | before use | cannot survive wait as an unprotected dereference | unresolved with lock key |
| Cursor yield retaining RID | yes | before yield/retention | when RID no longer usable | yes |
| Status lookup | no RID epoch | status pin/guard as applicable | lookup completion | status guard, not RID epoch |

## Version chains, slots, and pages

- Every surviving direct successor must be spliced around a reclaimed RID.
- If reverse-link proof fails, the slot remains DEAD.
- `REDIRECT_RESERVED` has no v1 reclamation role and remains unsupported.
- Compaction may move tuple bytes while preserving live RIDs.
- `DEAD → UNUSED` is one WAL-backed canonical page mutation.
- UNUSED coordinates are zero and the slot appears exactly once on `free_slot_head`.
- Whole-page heap reuse is optional, but any reinitialization that reuses `(PageNo,SlotId)` identities must satisfy all RID barriers.
- General extent allocation, PageNo renumbering, and whole-file shrink are not baseline requirements.
- File-level DROP retirement remains owned by §§4.7, 7.12.5, 21.9, and the object gates—not tuple vacuum.

---

# 10. Vacuum/reclaimer stages and identity

| Stage | Persistent mutation? | May wait? | Cancellation | Crash/restart |
|---|---:|---:|---|---|
| Resolve stable object/manifest | no | object claim only | cancel if RETIRING | restart from descriptors |
| Scan/classify | no | no transaction wait with latches/guard | safe-page boundary | no persistent effect |
| Normalize/freeze | WAL-backed | no physical wait while latched | complete/restore unit | redo/idempotent |
| Exact index cleanup | WAL-backed MTRs | ordinary B+ synchronization | stop before next unit | rerun idempotently |
| `NORMAL → DEAD` | WAL-backed | no | complete/restore | persistent DEAD |
| Epoch retirement/wait | runtime only | may defer/wait without latches | queue may disappear | re-enqueue DEAD |
| `prev` splicing | WAL-backed | defer on failed proof | complete/restore | redo/idempotent |
| `DEAD → UNUSED` | WAL-backed | only after barriers | complete/restore | free-list state recovered |
| Status cutoff publication | control-file durable | serialized reclaimer | old cutoff before publication | new cutoff monotonic afterward |
| Status frame/page retirement | physical maintenance | waits for pins/I/O | retry/defer | hole legal only below cutoff |

VACUUM is system maintenance, not an ordinary user transaction. Its WAL uses the Chapter-12 system-record ownership rules; it does not invent a user transaction chain.

FSM remains advisory. It may lag authoritative heap reclamation, but it must not advertise DEAD storage as reusable before heap metadata says so.

---

# 11. Crash matrix

| Crash state | Recovery outcome | Status lookup | RID reuse after reopen | READY |
|---|---|---|---:|---:|
| Freeze WAL durable, page unflushed | Redo freeze | Old status may be retired only if cutoff publication was safe | after ordinary barriers | yes |
| Normalize WAL durable, page unflushed | Redo normalization | Same | after barriers | yes |
| Freeze/normalize WAL not durable, new cutoff durable | **Underdefined unsafe state** | `RETIRED` may confront old metadata | no canonical result | finding |
| Some indexes cleaned, heap NORMAL | Rerun exact cleanup | Normal status rules | no | yes |
| Heap DEAD, stale index claimed | Forbidden by publication order | N/A | no | corruption/invariant |
| Heap DEAD, epoch wait in progress | Runtime wait disappears; re-enqueue | independent | after fresh grace | yes |
| Cutoff prerequisites complete, control old | Old cutoff | Old pages still required | independent | yes |
| Control new, status pages present | Leak only | `RETIRED` below cutoff | independent | yes |
| Control new, page deletion partial | Hole/present both legal | `RETIRED` | independent | yes |
| Control old, required page deleted | Forbidden by ordering | cannot prove outcome | no | no |
| DEAD not UNUSED | Preserve DEAD/re-enqueue | independent | not immediately | yes |
| RID reuse completed canonically | Recover UNUSED/new tuple state through WAL | independent | already complete | yes |
| Crash during epoch wait | No old readers survive | independent | fresh enqueue/grace | yes |
| Crash after index cleanup but before DEAD | Repeat cleanup | ordinary | no | yes |

---

# 12. Failure matrix

| Failure | Reclamation action | Continuation | Candidate safe? | Owner |
|---|---|---|---:|---|
| Status lookup I/O | Stop/propagate | Exact lower-layer policy | yes, unreclaimed | Chapters 9/39 |
| Required result is RETIRED | No mutation/guess | Noncontinuable | no dependency proof | §§9.13, 14.17.1 |
| INVALID/RESERVED required | No mutation | Corruption/invariant | yes, unreclaimed | Chapters 9/10/39 |
| Corrupt tuple/page | Stop; do not vacuum around | Noncontinuable/corruption | yes | Chapters 4/5/39 |
| Index cleanup failure | Leave NORMAL or prior safe units | Retry/fail pass | no RID reuse | Chapters 8/14 |
| WAL failure before authorization | Restore old unit | May fail/defer | yes | §12.12 |
| WAL failure after authorization | Complete or noncontinuable | No best-effort rollback | determined by owner | §12.12 |
| Control publication failure | Old cutoff | Retry possible if state known | yes | §§13.2,14.14 |
| Runtime cutoff cannot match durable control | Stop | Noncontinuable | no further work | §14.17.1 |
| Hole punch failure | Leave blocks allocated | Continue | yes | §14.14.2 |
| Checkpoint failure | Existing checkpoint/WAL retention remains | Continue or owning failure | yes if retention preserved | Chapters 13/39 |
| Epoch counter exhaustion | Disable new retirements/reuse | Other work continues | yes | §§4.3.2.5,14.6 |
| Runtime workspace exhaustion | Abort/defer unpublished unit | Usually continue | yes | §39 |
| Shutdown cancellation | Finish/restore current unit, quiesce | Controlled close | yes | Chapters 3/14 |

---

# 13. Concurrency and ordering matrix

| Race | Required order | Legal result | Forbidden result |
|---|---|---|---|
| Epoch enter vs retirement | Same epoch mutex | Reader gets `E≤R` or post-retirement `E>R` | Registration gap |
| Epoch exit vs grace | Count removal and predicate under mutex | Reclaimer waits or proceeds | Reuse while count remains |
| Writer lock wait vs RID reuse | **Missing rule** | Must preserve/restart physical identity safely | Reused lock key aliases new tuple |
| Index cleanup vs DEAD | All exact entries absent first | NORMAL partial cleanup or DEAD complete | DEAD with stale index |
| DEAD vs epoch | DEAD before retirement | Persistent nonreusable DEAD | Direct NORMAL→UNUSED |
| Epoch vs `prev` splice | Both required before UNUSED | Either may complete first | Reuse with surviving pointer |
| Freeze/normalize vs cutoff | Prerequisite must be crash-durable first | Old cutoff or safely reconstructible new state | New cutoff plus old dependent page |
| Cutoff vs status deletion | Durable/runtime cutoff first | Old page leak | Deletion under old cutoff |
| Checkpoint vs freeze | DPT sees old or complete new state | Redoable outcome | Lost reconstruction base |
| Checkpoint vs status reclaim | Cutoff-aware DPT filtering | Old or new coherent state | Checkpoint depends on deleted page |
| Shutdown vs reclaimer | Stop admission, finish/restore unit | Quiescent teardown | Destroy registry with live guard |
| VACUUM vs DROP | RETIRING cancels at unit boundary | DROP commits; unlink waits | Mutation after object retirement |

---

# 14. Cross-chapter consistency

| Owner | Result | Assessment |
|---|---|---|
| Chapter 3 lifecycle | CONSISTENT BUT SPECIALIZED | READY-only maintenance and quiescing are explicit |
| Chapter 4 IDs/control/exhaustion | CONSISTENT | Cutoff alignment and epoch no-wrap agree |
| Chapter 5 RID/slot/DEAD/FROZEN | CONSISTENT | DEAD/UNUSED and free-list geometry preserved |
| Chapter 6 FSM | CONSISTENT | Advisory only; heap authoritative |
| Chapter 7 pins/latches/file lifetime | CONSISTENT | Status-page drain and object retirement respect owners |
| Chapter 8 index references/MTR | CONSISTENT | Exact cleanup and MTR ownership preserved |
| Chapter 9 snapshots/status | FINDING | Active transaction can later create a below-cutoff dependency |
| Chapter 10 visibility/RETIRED | CONSISTENT | No guessed outcome |
| Chapter 11 stale RID/locks | FINDING | `TUPLE_WRITE` identity is not in RID-reuse barrier |
| Chapter 12 WAL/page-LSN | FINDING | No explicit WAL-durable-before-cutoff edge |
| Chapter 13 checkpoint/recovery | FINDING | New cutoff can outrun reconstructible prerequisite state |
| Chapter 15 DML | FINDING | New tuple fields use current TxnId without cutoff registration rule |
| §39 failures | CONSISTENT | Known/uncertain failure distinctions consumed correctly |
| §41 verification | CONSISTENT BUT INCOMPLETE | Broad vacuum obligations exist; detailed cutoff races are missing |

Previous-chapter regression result: Chapters 4–13 are not contradicted locally, but Chapter 14 fails to fully integrate three of their lifetime/durability contracts.

Chapter-13 compatibility result: status-page retirement itself agrees with recovery’s cutoff-aware DPT/redo rules; prerequisite tuple-page durability before cutoff remains unresolved.

---

# 15. Explicit cross-reference audit

All targets exist.

| Source | Target | Purpose | Precision/status |
|---|---|---|---|
| 14.6.1 | §11.10 | UNIQUE RID protection | Precise |
| 14.8 | §14.6.4 | DEAD restart | Precise |
| 14.12 | §5.3.2 | Free-slot chain | Precise |
| 14.14 | §14.17.1 | Guard/reclaimer coordination | Precise |
| 14.14 | §34.3.1 | Opaque StatsVersion | Precise |
| 14.14 | §9.12 | Absolute status mapping | Precise |
| Operation table | §§14.2–14.12 | Online VACUUM semantics | Acceptable range |
| Operation table | §14.14 | Status reclaimer | Precise |
| Operation table | §4.7 | Object/orphan cleanup | Broad but correct owner |
| Object gates | §11.13 | Gate/deadlock behavior | Precise |
| Object gates | §§7.12.5, 21.9, 4.7.7 | Physical drain/unlink | Precise |
| Object gates | §12.12 | Publication-unit restoration | Precise |
| VACUUM ownership | §14.11 | Revalidation | Precise |
| VACUUM ownership | §11.10 | UNIQUE current owner | Precise |
| ANALYZE | §34.3 | Snapshot semantics | Precise |
| ANALYZE | Chapter 34 | Approximate statistics | Broad but correct |
| ANALYZE | §39.1 | Failure effects | Precise |
| ANALYZE | §11.13 | Publication-gate graph | Precise |
| Statistics cache | §34.3.1 | StatsVersion ordering | Precise |
| Statistics cache | §39.1 | Cache failure | Precise |
| Statistics GC | §34.3.1 | Status independence | Precise |
| Status guard | §14.14 | Below-cutoff independence | Precise |
| Status guard | §14.14.2 steps 2–3 | Cutoff publication | Precise |
| Status guard | §9.13 | RETIRED invariant | Precise |
| Lifecycle | §39.1 and Chapter 3 | Shutdown/failure | Chapter 3 reference broad |
| Lifecycle | §3.3.6 | Shutdown | Precise |
| Lifecycle | §12.12 | Storage uncertainty | Precise |
| Lifecycle | §14.14.2 | Monotonic cutoff | Precise |
| Lifecycle | §4.7 | Cleanup pending | Broad but correct |
| Lock order | §11.13 | Unified graph | Precise |
| Forbidden #11 | Chapter 14 | RID reuse | Overly broad self-reference but understandable |
| Forbidden #16 | §9.14 | Terminal release | Precise |

No broken cross-reference was found.

---

# 16. Terminology and normative language

## Canonical terminology

| Term | Canonical meaning | Ambiguity assessment |
|---|---|---|
| Vacuum | Maintenance converting history into safe reclamation | Clear |
| Garbage candidate | Logically eligible for cleanup | Not yet physically reusable |
| DEAD | Semantically retired/index-cleaned, RID nonreusable | Clear |
| UNUSED | Canonically free-listed and reusable | Clear |
| Reclaimable | Context-sensitive; should specify status/page/RID | Mostly clear |
| Freezing/FROZEN | Replace committed old creator with sentinel | Clear |
| Normalization | Remove ineffective aborted xmax dependency | Clear |
| RETIRED | Runtime result saying outcome no longer needed | Clear |
| Status cutoff | Exclusive page-aligned reclaim-before value | Clear |
| Read epoch | Runtime RID-identity protection | Clear |
| `StatusHistoryGuard` | Runtime status-range retention claim | Clear |
| Snapshot horizon | Logical visibility reclamation boundary | Clear |
| RID reuse | Same physical RID may identify another tuple | Clear |
| Truncation | Used once in §14.1 despite no-shrink hole policy | Finding |
| Publication | WAL/control/runtime publication depending context | Context is generally explicit |

## Normative-language audit

| Section | Normative statement | Assessment |
|---|---|---|
| 14.3 | MUST NOT reclaim from worker snapshot alone | Correct |
| 14.4 | MUST NOT wait with heap/B+ latch | Correct |
| 14.9 | MUST NOT mark DEAD before all indexes absent | Correct |
| 14.17.1 | Concurrent workers MUST preserve RID order | Correct but incomplete regarding lock holders |
| 14.17.1 | Cache order MUST NOT regress | Correct |
| 14.18 | Declarative “never/only” invariants | Normatively adequate |
| 14.14.2 | Declarative mandatory order | Strong enough syntactically; missing prerequisite durability edge semantically |

No project-stage wording weakens a MUST. The problems are omissions outside the stated rules.

---

# 17. Temporality and document ownership

## Temporal-language classification

| Evidence/location | Classification | Assessment |
|---|---|---|
| “currently registered SQL snapshots” | A — runtime ordering | Valid |
| “current next_txn_id” | A — runtime state | Valid |
| “retried later” in 14.4/14.10/14.14 | A/B — runtime maintenance history | Valid |
| “after a crash” / “did not yet persist” | B — crash history | Valid |
| “current link” / “current owner” | A — runtime state | Valid |
| “later PageNos are never renumbered” | B — persistent identity history | Valid |
| “any future schema/index-manifest change” | D — durable v1 scope | Valid |
| “later guard observes G=C” | A — runtime ordering | Valid |
| “later failure after publication” | A — operation history | Valid |
| “vacuum may later trigger or assist” | F — project/roadmap chronology | Finding |
| “Background scheduling may later react” | F — project/roadmap chronology | Finding |

## Document-ownership table

| Material | Correct owner | Chapter-14 result |
|---|---|---|
| Reclamation predicates/invariants | Architecture | Correct |
| Exact race outcomes | Architecture | Correct |
| Deterministic pause/crash recipes | Verification | Not leaked |
| Vacuum implementation sequencing | Development | Two roadmap leaks |
| Current vacuum availability | Project State | No leakage |
| Historical test results | Devlog | No leakage |
| Linux syscall choice | Development/platform guidance | Leaked in §14.14.2 |
| Current source classes/files | Development/source | No leakage |

Global documentation assessment:

- Analytical rather than chronological: mostly yes.
- Current-state narration: none.
- DEVELOPMENT sequencing leakage: yes, two localized statements.
- VERIFICATION procedure leakage: none.
- PROJECT_STATE leakage: none.
- Devlog/history leakage: none.
- Reclamation terminology: precise except “truncation.”
- Rationale: strong around epochs, DEAD, RETIRED, sparse mapping, and two-slot safety; insufficient at the three missing cross-owner edges.
- Readable without implementation status: yes.
- Timeless canonical v1 contract: not yet, because semantic review and roadmap cleanup are required.

---

# 18. Analytical-depth assessment

| Mechanism | Result |
|---|---|
| Snapshot versus read epoch | Analytically sufficient |
| MVCC invisibility versus physical reuse | Analytically sufficient |
| DEAD versus UNUSED | Analytically sufficient |
| Index cleanup before DEAD | Analytically sufficient |
| Version-chain splicing | Analytically sufficient |
| Aborted-xmax normalization | Semantically clear; rationale adequate |
| Creator freezing | Semantically clear; exact cutoff durability integration incomplete |
| Status independence | Analytically sufficient as a universal predicate |
| RETIRED semantics | Analytically sufficient |
| Cutoff alignment/mapping | Analytically sufficient |
| Cutoff publication versus status-page deletion | Analytically sufficient |
| Cutoff publication versus prerequisite WAL durability | Analytical-depth/semantic finding |
| Status guard race | Analytically sufficient for vacuum lookup |
| Foreground DML versus cutoff | Semantic finding |
| RID epoch race | Analytically sufficient |
| Logical lock identity versus RID reuse | Semantic finding |
| Crash restart of DEAD | Analytically sufficient |
| Checkpoint/status-page retirement | Sufficient except prerequisite WAL force |
| Scheduling correctness versus policy | Clear, but roadmap wording inappropriate |

Implementation freedom is generally preserved. Deliberate exceptions are the mutex-based epoch baseline and one-at-a-time reclaimer/table-vacuum ownership. The Linux syscall example is unnecessary implementation coupling.

---

# 19. 110-item technical consistency matrix

Legend: `C` consistent, `S` consistent but specialized, `F` finding, `N/A` not assigned here.

| # | Topic | Result |
|---:|---|---|
| 1 | Reclamation owner | C |
| 2 | Snapshot vs read epoch | C |
| 3 | Epoch entry | C |
| 4 | Epoch exit | C |
| 5 | Epoch nesting | S |
| 6 | Epoch generation/exhaustion | C |
| 7 | StatusHistoryGuard owner | C |
| 8 | Guard lifetime | C |
| 9 | Guard vs read epoch | C |
| 10 | Vacuum horizon | C |
| 11 | RC horizon | C |
| 12 | RR horizon | C |
| 13 | Active-no-snapshot effect | F |
| 14 | DEAD predicate | C |
| 15 | Aborted insert | C |
| 16 | Committed delete | C |
| 17 | Update old version | C |
| 18 | Aborted xmax normalization | C |
| 19 | Aborted xmax proof | C |
| 20 | Aborted xmax concurrency | C |
| 21 | Creator freezing | C |
| 22 | Freeze visibility equivalence | C |
| 23 | Freeze invalid-status handling | C through Chapters 9–10 |
| 24 | FROZEN creator + normal xmax | C |
| 25 | Status independence | C |
| 26 | Cutoff meaning | C |
| 27 | Cutoff alignment | C |
| 28 | Cutoff maximum | C cross-section |
| 29 | RETIRED semantics | C |
| 30 | Lookup below cutoff | C |
| 31 | Lookup/cutoff race | C for existing lookups |
| 32 | Cutoff candidate inputs | F |
| 33 | Heap/catalog scan scope | S; universal proof fixed, mechanism free |
| 34 | Index indirect status dependency | C |
| 35 | Catalog status dependency | C |
| 36 | Status-page reclamation | C |
| 37 | Absolute status mapping | C |
| 38 | Reclaimed-page lookup | C |
| 39 | Status reclamation durability | F |
| 40 | Cutoff publication | F |
| 41 | Crash before cutoff publication | C |
| 42 | Crash after cutoff publication | F |
| 43 | Cutoff vs physical deletion | C |
| 44 | Checkpoint prerequisite | F: exact durable-WAL prerequisite missing |
| 45 | TXN_STATUS `F=rec_lsn` | C |
| 46 | WAL retention release | C after legal cutoff |
| 47 | Checkpoint/cutoff race | F at prerequisite durability edge |
| 48 | Checkpoint/freezing race | C before cutoff; F when cutoff follows without force |
| 49 | Checkpoint/status reclaim race | C |
| 50 | Vacuum WAL | C |
| 51 | Vacuum page_lsn | C |
| 52 | Pre-authorization failure | C |
| 53 | Post-authorization failure | C |
| 54 | DEAD semantics | C |
| 55 | DEAD vs UNUSED | C |
| 56 | DEAD publication | C |
| 57 | Index removal | C |
| 58 | Index cleanup failure | C |
| 59 | Multiple indexes | C |
| 60 | Heap sequential readers | C through broad epoch rule |
| 61 | RID reuse preconditions | F |
| 62 | Stale RID after reuse | F |
| 63 | SlotId reuse | C except lock gap |
| 64 | Compaction | C |
| 65 | Empty-page handling | S |
| 66 | Page reuse | S; optional but gated |
| 67 | File-level reclamation | C, owned elsewhere |
| 68 | `prev` rewrite/proof | C |
| 69 | Stale `prev` safety | C |
| 70 | REDIRECT_RESERVED | C/N/A for reclamation |
| 71 | Index cursor epoch | C |
| 72 | Materialized RID epoch | C |
| 73 | Writer candidate epoch | F across waits |
| 74 | Lock identity vs RID reuse | F |
| 75 | UNIQUE stale RID safety | C; waits restart full scan |
| 76 | Epoch registration race | C |
| 77 | Epoch exit race | C |
| 78 | Reclaimer wait policy | C; may wait/defer |
| 79 | Shutdown/cancellation | C |
| 80 | Crash clears epochs | C |
| 81 | Persistent DEAD restart | C |
| 82 | Crash during index cleanup | C |
| 83 | Crash during epoch wait | C |
| 84 | Crash during cutoff | F at durability edge |
| 85 | Crash during status delete | C |
| 86 | Recovery RETIRED | C |
| 87 | System-catalog vacuum | C through universal heap/catalog dependency proof |
| 88 | Long RR snapshot | C |
| 89 | Long read epoch | C |
| 90 | §14.17 scheduling wording | F |
| 91 | Status-history exhaustion | C through resource/TxnId rules |
| 92 | Slot exhaustion | C |
| 93 | Cutoff exhaustion | C |
| 94 | Reclaimer resource exhaustion | C |
| 95 | Error propagation | C |
| 96 | Corrupt tuple | C |
| 97 | RETIRED dependency violation | C |
| 98 | INVALID/RESERVED during vacuum | C |
| 99 | Lookup failure during vacuum | C |
| 100 | FROZEN positive retirement proof | C except durability edge |
| 101 | Normalized-xmax retirement proof | C except durability edge |
| 102 | Physical-removal retirement proof | C |
| 103 | Committed xmax dependency | C |
| 104 | Old xmin dependency | C |
| 105 | Status vs RID reclamation | C |
| 106 | Cutoff/control fallback safety | C |
| 107 | New cutoff/old heap durability | F |
| 108 | Stale index + new RID | C, explicitly forbidden |
| 109 | New RID + old `prev` | C, explicitly forbidden |
| 110 | Implementer invention | F |

---

# 20. Documentation-model matrix

| # | Question | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | Valid runtime temporal language preserved | CONSISTENT |
| 3 | No current implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No DEVELOPMENT sequencing | FINDING |
| 6 | No VERIFICATION procedure leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT |
| 9 | No source-layout coupling | CONSISTENT |
| 10 | Snapshot/read-epoch terminology | CONSISTENT |
| 11 | DEAD/reusable terminology | CONSISTENT |
| 12 | FROZEN/RETIRED distinction | CONSISTENT |
| 13 | Status-independent terminology | CONSISTENT |
| 14 | Cutoff meaning | CONSISTENT |
| 15 | Status vs RID reclamation | CONSISTENT |
| 16 | Checkpoint/cutoff rationale | FINDING |
| 17 | Epoch/reuse rationale | CONSISTENT, but lock scope incomplete |
| 18 | Freezing rationale | CONSISTENT |
| 19 | §14.17 scheduling timelessness | FINDING |
| 20 | Readable without implementation status | CONSISTENT |

---

# 21. Complete findings

## BLOCKING B14-1 — New below-cutoff dependency from an active transaction

- Section: §14.17.1, “Status-history guard and cutoff publication,” cross-section with §§9.9, 9.13 and 15.2–15.4.
- Evidence: [lines 11412–11418](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11412):
  “Transaction existence without such a dependency does not pin the cutoff.”
- Severity: BLOCKING
- Type: STATUS CUTOFF
- Scope: Cross-section/cross-chapter
- Arithmetic: If active transaction `T` has `T < C`, a later tuple written as `xmin=T` lies below the exclusive cutoff and therefore resolves to `RETIRED` after runtime active state disappears.
- Explanation: RC permits an active transaction to have no registered snapshot between statements. Chapter 15 later permits that same transaction to write tuples using its existing TxnId. No protocol requires a write-capable transaction or first persistent write to register a status-correctness dependency under the cutoff coordinator.
- Canonical comparison: §9.13 returns active state before cutoff while the process runs, but after commit/restart persistent metadata needs terminal status. Chapter 15 writes `xmin=current TxnId`.
- Consequence: A valid transaction can create persistent metadata whose required outcome has already been retired. After restart, visibility encounters `RETIRED` and must report corruption/noncontinuability.
- Correct owner: Architecture, principally Chapter 14 integrated with Chapters 9 and 15.
- Future action: **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**
- Required decision: Define exact registration and linearization for transactions that may introduce a persistent TxnId reference, or forbid such writes once their TxnId is below the cutoff.

## BLOCKING B14-2 — Cutoff may outrun durable freeze/normalization state

- Section: §14.14.2 and §14.17.1.
- Evidence: [lines 11049–11059](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11049) order proof directly before durable control publication, but does not require WAL through the prerequisite page mutations to be durable. [Line 11414](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11414) says “existing persistent freeze proof” without defining its durability observation.
- Severity: BLOCKING
- Type: CHECKPOINT/RETENTION
- Scope: Cross-section/cross-chapter
- Arithmetic: N/A
- Explanation: A page can be normalized/frozen in a dirty frame with appended but unflushed WAL. `database.control` can then be independently `fdatasync`ed. A crash may preserve the new cutoff while losing the WAL and retaining the old data-page image.
- Canonical comparison: §12.17 requires WAL durability before data-page writeback, not before an independent control-file update. §13.2.4 synchronizes `database.control` but does not transitively force unrelated WAL.
- Consequence: Recovery may observe a durable new cutoff and old tuple metadata below it, while the WAL needed to make that metadata status-independent is absent.
- Correct owner: Architecture, Chapter 14 publication protocol with Chapters 12–13.
- Future action: **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**
- Required decision: Define whether cutoff publication requires WAL durability through every prerequisite normalization/freeze LSN, a covering installed checkpoint/data-page durability state, or another exact recovery-safe publication proof.

## BLOCKING B14-3 — `TUPLE_WRITE` identity is absent from the RID-reuse barrier

- Section: §§14.5–14.6 and §14.17.1, cross-section with §§11.2–11.4.
- Evidence: [lines 11252–11260](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11252) list snapshots, latches, guards, and ReadEpoch protection; [lines 11262–11270](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11262) omit logical RID lock ownership from the reuse order. [Lines 11515–11517](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11515) prohibit waiting while a read-epoch guard is held.
- Severity: BLOCKING
- Type: RID REUSE
- Scope: Cross-section/cross-chapter
- Arithmetic: N/A
- Explanation: `TUPLE_WRITE` is keyed by physical RID. A waiter must release short physical protection, retain the RID as a lock key, then refetch/revalidate. Vacuum does not consult lock ownership before `DEAD → UNUSED`.
- Canonical comparison: Chapter 11 requires post-wait revalidation but defines no generation-bearing RID identity. Chapter 14’s purpose is to prevent a stale physical RID from rebinding to a different tuple.
- Consequence: The RID may be reused while a legal lock holder/waiter still names it, potentially transferring a logical lock to an unrelated tuple or allowing stale-target revalidation against a new identity.
- Correct owner: Architecture, Chapter 14 integrated with Chapter 11.
- Future action: **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**
- Required decision: Add logical-lock/waiter quiescence to reuse, define a lock-to-epoch handoff, or require a restart protocol that cannot retain/rebind the old RID.

## MINOR M14-1 — “Truncation” terminology conflicts with keep-size reclamation

- Section: §14.1.
- Evidence: [line 10608](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:10608): “transaction-status retention/truncation eligibility.”
- Severity: MINOR
- Type: TERMINOLOGY
- Scope: Cross-section
- Explanation: §14.14 mandates sparse hole punching while keeping logical file length and PageNos unchanged.
- Consequence: “Truncation” can suggest suffix shrink or renumbering that the normative protocol forbids.
- Correct owner: Architecture.
- Future action: **H. TERMINOLOGY NORMALIZATION.**

## MINOR M14-2 — Linux syscall implementation coupling

- Section: §14.14.2.
- Evidence: [lines 11074–11080](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11074): `fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)`.
- Severity: MINOR
- Type: IMPLEMENTATION COUPLING
- Scope: Local
- Explanation: The architectural result is a keep-size sparse page deallocation. The exact Linux API is platform/development guidance.
- Consequence: Unnecessarily couples the canonical architecture to one syscall spelling.
- Correct owner: DEVELOPMENT/platform implementation guidance.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX.**

## MINOR M14-3 — Roadmap wording in FSM/statistics boundary

- Section: §14.16.
- Evidence: [line 11144](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11144): “vacuum may later trigger or assist that subsystem.”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local
- Explanation: “Later” is project chronology, not runtime ordering.
- Consequence: Makes live architecture depend on development sequence.
- Correct owner: Timeless Architecture wording or DEVELOPMENT sequencing.
- Future action: **B. TIMELESSNESS REWRITE.**

## MINOR M14-4 — Background-scheduler roadmap wording

- Section: §14.17.
- Evidence: [line 11158](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:11158): “Background scheduling may later react…”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local
- Explanation: The permitted policy inputs can be stated timelessly; “later” narrates implementation evolution.
- Consequence: DEVELOPMENT-owned roadmap language appears in Architecture.
- Correct owner: Timeless Architecture scope or DEVELOPMENT.
- Future action: **B. TIMELESSNESS REWRITE.**

No MAJOR or EDITORIAL findings were assigned.

---

# 22. Frozen architecture semantic questions

1. **Foreground transaction status-dependency admission:** What exact coordinator registration prevents a transaction whose TxnId is below a newly advancing cutoff from later publishing a tuple/catalog `xmin` or `xmax` using that TxnId?

2. **Cutoff prerequisite durability:** What exact durable observation—WAL forced through prerequisite LSNs, durably clean pages, installed checkpoint, or another proof—must precede `database.control` cutoff publication?

3. **Logical RID lock versus physical reuse:** What exact handoff or quiescence rule prevents `TUPLE_WRITE(TableId,RID)` holders/waiters from surviving reuse of the same RID for a different tuple?

These require frozen semantic decisions; local wording edits are insufficient.

---

# 23. Follow-up verification gaps

Existing verification adequately owns:

- index cleanup before DEAD;
- partial-cleanup crash idempotence;
- persistent DEAD restart;
- epoch grace and counter exhaustion;
- version-chain splicing;
- long-running snapshot behavior;
- basic freeze/normalization visibility equivalence;
- numeric cutoff encoding through the Chapter-13 control fixture.

Follow-up gaps:

1. **Status cutoff durability matrix:** deterministic old/new control, dirty old heap page, durable/nondurable freeze WAL, checkpoint old/new, and restart outcomes. This must follow resolution of B14-2.

2. **Foreground DML/cutoff race:** active RC transaction with no snapshot, cutoff proof/publication, then first persistent write/commit/restart. This must follow B14-1.

3. **Logical RID lock/reuse race:** TUPLE_WRITE holder/waiter versus index cleanup, DEAD, epoch grace, UNUSED, and post-wait revalidation. This must follow B14-3.

4. **Status-history coordinator procedures:** guard-first/reclaimer-first barriers, multiple guards, status pins, frame/DPT retirement, hole-punch failure, and runtime-cutoff publication failure.

5. **Freeze/normalization error matrix:** COMMITTED/ABORTED/IN_PROGRESS/RETIRED/INVALID/RESERVED/I/O/corruption, with CRC-valid persistent fixtures and WAL crash boundaries.

6. **§14.17.1 maintenance races:** VACUUM/ANALYZE/DROP/RETIRING/cutoff/checkpoint required outcomes are not presently given a complete dedicated procedural owner.

These are verification gaps, not additional architecture findings.

---

# 24. Direct high-priority answers

| Question | Answer |
|---|---|
| Snapshot/read-epoch ambiguity? | No for readers; yes only at writer-lock integration |
| StatusHistoryGuard lifetime ambiguity? | No for vacuum passes |
| DEAD/reusable conflation? | No |
| Creator-freezing ambiguity? | Local eligibility clear; publication durability incomplete |
| Aborted-xmax ambiguity? | No locally; publication durability incomplete |
| Status-independence proof gap? | Yes for future dependencies from active transactions |
| RETIRED-as-guessed-outcome path? | No |
| Cutoff inclusive/exclusive ambiguity? | No; exclusive |
| Cutoff alignment/max ambiguity? | No architecture-wide |
| Cutoff before durable proof path? | Yes |
| Status-page deletion before safe cutoff? | No |
| Old-control fallback requiring deleted page? | Prevented for normal torn updates; unusable fallback must fail |
| New cutoff with old dependent heap metadata? | Yes, due missing WAL durability edge |
| TXN_STATUS `F` retention gap? | No for nonretired pages |
| RID reuse while index/epoch survives? | Explicitly forbidden |
| RID reuse while logical RID lock survives? | Yes, unresolved |
| Stale `prev` → new tuple alias? | Prevented |
| Lock identity → reused RID alias? | Possible |
| Checkpoint/reclaimer ambiguity? | At prerequisite WAL durability edge |
| Crash/restart DEAD ambiguity? | No |
| §14.17 project-roadmap wording? | Yes |
| Correctness-relevant implementer invention? | Yes, three decisions |
| Project-time/current-state wording? | Project-time yes; current-state no |
| DEVELOPMENT-owned material? | Yes |
| VERIFICATION-owned procedure leakage? | No |
| PROJECT_STATE-owned material? | No |
| Devlog/history material? | No |
| Ambiguous terminology? | Local “truncation” issue |
| Analytically underexplained boundary? | Yes, three safety edges |
| Timeless canonical v1 contract? | Not until semantic review and targeted cleanup |

---

# 25. Recommended next actions

Recommended next action: **frozen semantic review required**.

Order:

1. Resolve the three frozen architecture questions.
2. Perform a targeted Chapter-14 architecture edit for those decisions plus the four minor documentation findings.
3. Synchronize Chapter-14 verification procedures.
4. Only then treat Chapter 14 as closed.

Recommended Chapter-15 read-only review scope, based on the actual boundary:

- DML acquisition and retention of `TableWriterGate`, `TUPLE_WRITE`, and `UNIQUE_KEY`;
- post-wait stale-RID revalidation and its future Chapter-14 reuse integration;
- first persistent write and status-dependency registration;
- INSERT/UPDATE/DELETE WAL publication ordering;
- update `prev` links and vacuum compatibility;
- abort residue and vacuum eligibility;
- COMMIT/ABORT terminal publication;
- RC retry boundary, especially §§15.7.2–15.7.3;
- failure ownership under §39.

Chapter 15 review was not started.

Out-of-scope observations:

- §§15.7.2–15.7.3 unchanged and deferred to Chapter-15 review.
- §31.7 unchanged.
- Appendix C unchanged.
- Appendix C also contains project-sequencing language about background vacuum, but it was not adjudicated here.

---

# 26. Repository-state and read-only guarantee

Initial Git state:

```text
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 258db4277a5e2a57edec9d82a375636183e208c4
```

Final Git state:

```text
git diff --check:            passed, no output
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 258db4277a5e2a57edec9d82a375636183e208c4
```

Files modified by audit: **NONE**.

Repository-state-change assessment: no tracked, untracked, staged, or HEAD change occurred during the review. No pre-existing material was modified or staged.

No build, test, benchmark, implementation, formatting, review artifact, devlog, milestone, commit, or staging operation occurred.

**Phase 2 remains NOT STARTED / NOT AUTHORIZED.**