# Chapter 12 Frozen Revision — Clean

The authorized D12-M1 decision is integrated, and F12-2 through F12-4 are resolved.

- Chapter 12 architecture: **CLEAN**
- Frozen semantic questions: **RESOLVED — NONE OPEN**
- Chapter 12 verification: **SYNCHRONIZATION PENDING**
- Chapter 13 review: **NOT STARTED**

## Git state

Initial:

- Status: one pre-existing untracked artifact:
  `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 12/D12-M1.md`
- Index: clean
- HEAD: `38f84963d0e663358187a4620c268040dbf556fc`

The pre-existing artifact was not read, modified, removed, or staged.

## D12-M1 — Final WAL grammar

The final rule is:

```text
PAGE_DELTA.patch_count >= 1
BTREE_MTR.page_count >= 1
BTREE_MTR PATCH_SET.patch_count >= 1
```

A complete recognized-v1 record violating one of these rules is malformed WAL and follows the canonical corruption path. It is not a torn tail, unsupported format, legal no-op, barrier, heartbeat, or position marker.

### PAGE_DELTA

Old grammar:

- Encoded `patch_count`.
- Required each encoded patch to be nonempty.
- Did not define whether `patch_count == 0` was legal.

Corrected [§12.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9111):

- `patch_count MUST be at least 1`.
- Writers `MUST NOT` emit zero-patch PAGE_DELTA.
- Decoders `MUST` reject zero as malformed recognized-v1 WAL.
- The record cannot enter redo or advance `page_lsn`.
- Existing order, bounds, overlap, header-exclusion, length, and trailing-byte rules remain unchanged.
- Rationale now states that PAGE_DELTA represents an actual existing-page mutation.

### BTREE_MTR

Old grammar:

- Encoded `page_count`.
- Required parsing exactly that many entries.
- Did not define whether `page_count == 0` was legal.

Corrected [§12.10.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9232):

- `page_count MUST be at least 1`.
- Writers `MUST NOT` emit zero-page BTREE_MTR.
- Decoders `MUST` reject zero as malformed recognized-v1 WAL.
- Zero-page MTR is explicitly not an empty structural transaction, no-op, barrier, or future format.
- MTR atomicity, common LSN, membership, size, and publication rules are unchanged.

### Nested PATCH_SET

Old grammar:

- Encoded `patch_count`.
- Reused PAGE_DELTA patch rules.
- Did not define whether zero patches were legal.

Corrected grammar:

- `patch_count MUST be at least 1`.
- Writers `MUST NOT` emit zero-patch PATCH_SET.
- Decoders reject it through the corruption path.
- It is explicitly distinguished from torn-tail and unsupported-format outcomes.
- Existing `data_length = 8 + sum(8 + patch.length)`, reserved-zero, ordering, bounds, and overlap rules remain unchanged.

Recovery now has a deterministic decoder oracle:

| Input | Classification |
|---|---|
| Complete recognized-v1 record with forbidden zero count | Malformed WAL/corruption |
| Incomplete final physical record | Torn tail under Chapter 13 |
| Complete unknown record type | `UNSUPPORTED_WAL_FORMAT` |
| Recognizable future owning format | Unsupported format |

No no-op, heartbeat, barrier, or WAL-position-marker semantics were introduced.

D12-M1 status: **RESOLVED AND INTEGRATED**.

## F12-2 — Append-buffer cleanup

Original [§12.12.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9686) prescribed:

- an initial configurable 8 MiB target;
- an initial one-mutex realization;
- later per-thread, completion-marker, and ring-buffer alternatives.

The corrected section now states only the architectural contract:

- exact bytes retained for append, physical writing, durability, and retry;
- private record construction;
- contiguous total append order;
- no holes;
- atomic valid-end publication;
- retained-byte lifetime;
- concurrent append/flush safety;
- shutdown draining;
- uncertainty handling.

Runtime data structures, capacity policy, and synchronization strategy are expressly not architecturally fixed. Lock-free operation is not required.

F12-2 status: **RESOLVED**.

## F12-3 — Flush-interval cleanup

Original [§12.13](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9697) specified an approximate initial 10 ms background interval.

That tuning default was removed. The section now states that background-WAL scheduling mechanism and cadence are not architecturally fixed.

Unchanged:

- monotonic `durable_lsn`;
- complete-prefix durability;
- namespace prerequisite;
- cross-segment synchronization;
- waiter target semantics;
- coalescing freedom;
- failure propagation and lifecycle ownership.

F12-3 status: **RESOLVED**.

## F12-4 — Group-commit example

Original values:

```text
T1 -> 1000
T2 -> 1100
T3 -> 1250
```

Corrected [§12.14](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9719):

```text
T1 -> 1000
T2 -> 1104
T3 -> 1256
```

All values are divisible by eight. The flush target and `durable_lsn` example now use `1256`.

Group-commit semantics are unchanged: one synchronization may satisfy multiple waiters, but each transaction’s C3 condition remains tied to its own `commit_lsn`.

F12-4 status: **RESOLVED**.

## Invariant synchronization

[§12.18](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9854) now records:

- PAGE_DELTA contains at least one nonempty patch.
- Every BTREE_MTR has at least one affected page.
- Every nested PATCH_SET contains at least one nonempty patch.

No byte layouts were duplicated.

## Technical regression

| Contract | Result |
|---|---|
| WAL namespace, names, segment size/header/prefix | Unchanged |
| 48-byte record header and CRC coverage | Unchanged |
| Record registry and codes | Unchanged |
| LSN meaning, alignment, first LSN, arithmetic | Unchanged |
| `page_lsn`, `rec_lsn`, `durable_lsn` | Unchanged |
| Reservation and no-hole rule | Unchanged |
| Valid-end authorization boundary | Unchanged |
| Durable-prefix semantics | Unchanged |
| C3 durable COMMIT | Unchanged |
| ABORT A0–A4 | Unchanged |
| Read-only terminal-WAL exception | Unchanged |
| No user-DML CLR/physical undo | Unchanged |
| MTR atomicity/common LSN/no-flush | Unchanged except minimum count domain |
| PAGE_INIT/publication | Unchanged |
| TXN_STATUS `F < T`, `rec_lsn=F`, `page_lsn=T` | Unchanged |
| WAL-before-data | Unchanged |
| Known/uncertain failure classes | Unchanged |
| Numeric/resource exhaustion | Unchanged |
| Shutdown/recovery boundary | Unchanged |

## Local reread answers

| # | Question | Answer |
|---:|---|---|
| 1 | PAGE_DELTA explicitly requires `patch_count >= 1`? | YES |
| 2 | Writer forbids zero-patch PAGE_DELTA? | YES |
| 3 | Decoder rejects it? | YES |
| 4 | Classified as malformed/corrupt v1 WAL? | YES |
| 5 | Distinguished from torn tail? | YES |
| 6 | Distinguished from unsupported format? | YES |
| 7 | Can it advance `page_lsn`? | NO |
| 8 | BTREE_MTR requires `page_count >= 1`? | YES |
| 9 | Writer forbids zero-page MTR? | YES |
| 10 | Decoder rejects it? | YES |
| 11 | Classified as malformed/corrupt v1 WAL? | YES |
| 12 | Nested PATCH_SET requires `patch_count >= 1`? | YES |
| 13 | Zero-patch PATCH_SET rejected? | YES |
| 14 | Any no-op/barrier semantics introduced? | NO |
| 15 | Existing upper bounds unchanged? | YES |
| 16 | Existing length arithmetic unchanged? | YES |
| 17 | MTR atomicity unchanged? | YES |
| 18 | Valid PAGE_DELTA semantics unchanged? | YES |
| 19 | §12.12.5 free of “initial implementation”? | YES |
| 20 | Free of one-mutex prescription? | YES |
| 21 | Free of later-alternatives roadmap? | YES |
| 22 | Append/no-hole/publication semantics preserved? | YES |
| 23 | §12.13 free of the 10 ms default? | YES |
| 24 | `durable_lsn` semantics unchanged? | YES |
| 25 | Example LSNs all eight-byte aligned? | YES |
| 26 | Group-commit semantics unchanged? | YES |
| 27 | C3 durable COMMIT unchanged? | YES |
| 28 | WAL-before-data unchanged? | YES |
| 29 | PAGE_INIT unchanged? | YES |
| 30 | TXN_STATUS protocol unchanged? | YES |
| 31 | Any record code/header field changed? | NO |
| 32 | Any unrelated Chapter 12 behavior changed? | NO |

## Documentation-model assessment

- Project chronology introduced: **NO**
- Current implementation status introduced: **NO**
- DEVELOPMENT sequencing remains in corrected scopes: **NO**
- VERIFICATION procedure added: **NO**
- PROJECT_STATE facts added: **NO**
- History/devlog material added: **NO**
- WAL grammar deterministic: **YES**
- Persistent-format rationale sufficient: **YES**
- §12.12.5 implementation freedom preserved: **YES**
- Flush scheduler/timing freedom preserved: **YES**
- Valid runtime/WAL temporal language preserved: **YES**
- Timeless canonical v1 result: **YES**

## Cross-chapter compatibility

Chapters 3–11 remain compatible:

- Chapter 3 lifecycle/NONCONTINUABLE: unchanged.
- Chapter 4 alignment, format classification, and corruption: compatible.
- Chapters 5–6 page mutation WAL: compatible.
- Chapter 7 WAL-before-data/writeback: unchanged.
- Chapter 8 BTREE_MTR: compatible with the nonzero-participant rule.
- Chapter 9 C0–C6/A0–A4: unchanged.
- Chapter 10 recovered terminal authority: unchanged.
- Chapter 11 terminal lock release: unchanged.

Chapter 13 compatibility: **CONSISTENT**. Its existing rule that malformed required WAL is corruption now receives an exact Chapter 12 count-domain oracle. Chapter 13 was not modified.

Frozen architecture semantic questions: **RESOLVED — NONE OPEN**.

## Pending verification synchronization

Still pending:

1. Exact WAL segment/header/record codec fixtures.
2. Record-family payload codecs, including all three nonzero count boundaries.
3. Durable-prefix/group-commit procedures.
4. WAL-tail/segment-inventory recovery fixtures.
5. TXN_STATUS two-record crash-prefix matrix.

No verification work was performed.

Chapter 13 review remains **NOT STARTED**.

Out-of-scope wording remains unchanged:

- §14.17
- §§15.7.2–15.7.3
- §31.7
- Appendix C

## Diff audit

Task-modified file:

- [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

Hunk classifications:

| Classification | Content |
|---|---|
| A | PAGE_DELTA minimum count and rationale |
| B | BTREE_MTR minimum page count |
| C | Nested PATCH_SET minimum patch count |
| D | Corruption/no-op classification and §12.18 synchronization |
| E | Initial append-buffer roadmap removal |
| F | Timeless append-buffer implementation freedom |
| G | Background flush tuning-default removal |
| H | Legal aligned group-commit LSNs |
| I | No wrapping-only hunk required |

Only `docs/ARCHITECTURE.md` was task-modified.

## Final repository state

- `git status --short`:
  - `M docs/ARCHITECTURE.md`
  - unchanged pre-existing untracked `docs/reviews/.../D12-M1.md`
- Index: clean
- HEAD: `38f84963d0e663358187a4620c268040dbf556fc`
- `git diff --check`: passed with no output

No pre-existing material was modified or staged. No external repository change occurred during the task beyond the already-present untracked artifact.

No implementation, source, test, build, benchmark, staging, commit, devlog, or review-artifact work occurred.

Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.