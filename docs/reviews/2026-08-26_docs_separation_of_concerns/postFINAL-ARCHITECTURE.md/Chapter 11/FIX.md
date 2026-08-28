# Chapter-11 targeted-fix verdict

**CHAPTER 11 ARCHITECTURE — CLEAN**

All three targeted findings are resolved. No frozen semantic question arose. Chapter-11 verification synchronization remains pending, so the chapter is not fully closed.

## Git baseline

- Initial working tree: clean
- Initial index: clean
- Initial HEAD: `098775bb0a985a9033c1667eee4ec8d34b84b9a5`
- Pre-existing state: no tracked, staged, or untracked changes to preserve

## F11-1 — Lock-table wording

Original §11.12 prescribed:

> “The initial LockManager MAY use … `hash map<LockKey, LockQueue>` … protected by a conventional mutex.”

It also required the abstraction to allow “later” sharding and additional modes.

[Corrected §11.12](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:8418) now states a timeless contract:

- process-local runtime mapping from exact `LockKey` to conceptual `LockQueue`;
- current owner plus FIFO waiter list;
- unchanged grant/same-owner/wait compatibility;
- implementation freedom for runtime data structures and synchronization;
- atomic cancellation and no late grant after transaction ineligibility;
- unified wait-for-graph integration;
- terminal-duration ownership and coherent wake-up;
- concurrent and parallel-ready correctness.

Preservation results:

| Requirement | Result |
|---|---|
| FIFO queue semantics | Preserved |
| Compatibility | Preserved |
| Same-owner/reentrant handling | Preserved |
| Incompatible-request waiting | Preserved |
| Cancellation/no-late-grant | Preserved and stated explicitly |
| Deadlock-graph integration | Preserved |
| Terminal lock lifetime | Preserved |
| V1 exclusive-mode scope | Unchanged |
| Shared/intention/range modes | Not promoted into v1 |
| Container/mutex prescription | Removed |
| Implementation roadmap | Removed |
| Parallel-ready freedom | Timelessly preserved |

**F11-1: RESOLVED**

## F11-2 — Historical deadlock wording

Original:

> “This is the existing youngest-transaction policy, now applied across every registered resource family.”

[Corrected §11.13.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:8632):

> “This deterministic victim policy applies uniformly across every registered resource family.”

The graph, edge, synchronous detection, victim, `DEADLOCK_DETECTED`, `MUST_ABORT`, release, and survivor-revalidation rules are unchanged.

**F11-2: RESOLVED**

## F11-3 — Cyclic-SCC precision

Original invariant:

> “the initial victim is the highest TxnId in the cycle.”

[Corrected §11.15 invariant 20](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:8795):

> “each cyclic strongly connected component selects its highest normal TxnId as the victim.”

An SCC containing overlapping simple cycles therefore produces the single canonical SCC-level victim decision, not independent cycle-local decisions.

**F11-3: RESOLVED**

## Local re-review answers

| # | Question | Answer |
|---:|---|---|
| 1 | Free of “initial LockManager” sequencing? | YES |
| 2 | Free of hash-map/mutex prescription? | YES |
| 3 | Free of later-sharding/shared-mode roadmap? | YES |
| 4 | Queue/compatibility/wait semantics unchanged? | YES |
| 5 | V1 lock-mode scope unchanged? | YES |
| 6 | Implementation freedom timeless? | YES |
| 7 | Parallel-ready compatibility retained? | YES |
| 8 | “Existing” design-history wording removed? | YES |
| 9 | “Now applied” wording removed? | YES |
| 10 | Highest-normal-TxnId-per-cyclic-SCC retained? | YES |
| 11 | §11.15 uses cyclic-SCC precision? | YES |
| 12 | Overlapping-cycle interpretation canonical? | YES |
| 13 | Any deadlock behavior changed? | NO |
| 14 | Any lock/conflict/uniqueness behavior changed? | NO |

## Documentation-model assessment

| Question | Result |
|---|---|
| Project chronology in corrected scopes | NONE |
| Current implementation narration | NONE |
| DEVELOPMENT sequencing | NONE |
| VERIFICATION procedure leakage | NONE |
| PROJECT_STATE facts | NONE |
| Devlog/history material | NONE |
| Architecture semantics preserved | YES |
| Implementation freedom preserved | YES |
| Valid runtime temporal language preserved | YES |
| Analytical/descriptive result | YES |
| Independent of implementation status | YES |
| Timeless canonical v1 contract | YES |

Normative strength remains appropriate: semantic requirements use `MUST`; implementation realization uses `MAY` only for architecture-compatible freedom.

## Technical regression

All unchanged:

- `TUPLE_WRITE(TableId, physical RID)` identity and exclusive semantics
- `UNIQUE_KEY(IndexId, canonical encoded non-NULL key)` identity
- lock/latch separation
- terminal-duration ownership
- current-owner and status-error handling
- READ COMMITTED conflict/retry boundary
- REPEATABLE READ serialization failure
- lost-update prevention
- uniqueness and NULL semantics
- unified database-local wait-for graph
- blocker/predecessor edges
- synchronous SCC detection
- highest-normal-TxnId victim per cyclic SCC
- `DEADLOCK_DETECTED` and `MUST_ABORT`
- cancellation/no-late-grant behavior
- wake-up revalidation
- C4→C5 and A2→A3 release ordering
- shutdown/drain lifecycle

Compatibility remains intact with Chapters 5, 7–10, 14–15, and §39.

## Protected material

Confirmed unchanged:

- `docs/VERIFICATION.md`
- `docs/DEVELOPMENT.md`
- `docs/PROJECT_STATE.md`
- Chapter 12
- Chapter 14
- Chapter 15
- Chapter 31
- Appendix C
- source, tests, benchmarks, and build files

## Pending verification synchronization

Status: **PENDING**

The five retained areas are:

1. `TUPLE_WRITE` invalid/current-owner status matrix.
2. UNIQUE current-state invalid/error fixtures.
3. Lock/wait/graph resource-exhaustion boundaries.
4. Recovery clearing runtime holders, waiters, and graph edges without SQL lock/UNIQUE replay.
5. FIFO, same-owner/reentrant, and no-late-grant queue checks.

Out-of-scope wording in §§14.17, 15.7.2–15.7.3, and 31.7 remains unchanged.

Chapter-12 review: **NOT STARTED**

## Diff and final repository state

Files changed:

- `docs/ARCHITECTURE.md` only

Hunk classification:

| Hunk | Classification |
|---|---|
| §11.12 removal | A — container/sequencing removal |
| §11.12 replacement | B — timeless implementation freedom |
| §11.13.4 | C — historical wording removal |
| §11.15 | D — SCC victim precision |
| Wrapping | E — limited to the authorized replacements |

Final checks:

- `git status --short`: `M docs/ARCHITECTURE.md`
- Index: clean
- HEAD unchanged: `098775bb0a985a9033c1667eee4ec8d34b84b9a5`
- `git diff --check`: passed
- Diff: 1 file, 13 insertions, 18 deletions
- External repository changes detected: none
- Staged files: none
- Pre-existing material modified: none

No implementation work, builds, tests, benchmarks, staging, commits, devlog, or review artifacts were performed.

**Phase 2 remains NOT STARTED / NOT AUTHORIZED.**