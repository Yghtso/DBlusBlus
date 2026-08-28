# Chapter 10 verification-synchronization verdict

**CLOSED — Chapter 10 MVCC visibility verification methodology is synchronized.**

The canonical methodology is integrated into [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:3761).

- Atomic obligations: **96**
- COMPLETE: **96**
- PARTIAL: **0**
- MISSING: **0**
- CONTRADICTORY: **0**
- Frozen architecture questions: **none**
- Chapter 11 review: **NOT STARTED**

## Git baseline

- Initial status: clean
- Initial index: clean
- Initial HEAD: `2c41e7a8c9579951f2c68e7a1168429ca2ead70b`
- No pre-existing changes or untracked items required preservation.

## Organization

The existing `MVCC Visibility Tests` owner was expanded with:

- Deterministic harness, fixture canonicality, and independent oracle
- Creator and deleter valid-state regression
- Status-result and lookup-failure propagation
- SELF command boundaries and causal precheck
- Statement effects and UPDATE version pairs
- Isolation, retry, recovery, and retirement cross-checks
- Heap and index candidate error propagation
- Chapter 10 high-level domain/case matrix
- Chapter 10 architecture-obligation coverage map

This keeps Chapter 10 semantics in Architecture while Verification owns fixtures, schedules, oracles, matrices, and error propagation.

## Deterministic harness and oracle

Fixtures independently control:

```text
xmin / xmax / cmin / cmax
persisted versus runtime-only provenance
snapshot owner / CommandId / xmax / active
TxnStatus result or injected lookup failure
heap, sequential-scan, or B+ candidate source
```

The observable domain is implementation-neutral:

```text
VISIBLE
INVISIBLE
ERROR with canonical family
```

The independent evaluator performs:

1. Structural and owner validation.
2. SELF command-causality validation.
3. Creator evaluation.
4. Deleter evaluation if the creator is visible.
5. Exact error propagation.

Production visibility code is never its own oracle. Concurrency and retry cases require deterministic barriers, not sleeps.

## Creator methodology

| Creator case | Expected result |
|---|---|
| FROZEN | VISIBLE without status access |
| SELF `cmin<C` | VISIBLE |
| SELF `cmin==C` | INVISIBLE |
| SELF `cmin>C` | ERROR |
| COMMITTED before snapshot | VISIBLE |
| COMMITTED active at capture | INVISIBLE |
| COMMITTED at/above `xmax` | INVISIBLE |
| IN_PROGRESS | INVISIBLE |
| ABORTED | INVISIBLE |
| RETIRED dependent | `CORRUPT_HEAP` |
| INVALID dependent | `CORRUPT_HEAP` |
| RESERVED dependent | Valid decode, then `CORRUPT_HEAP` |
| Lookup failure | Exact lower-layer error |

Lookup failure is directly distinguished from creator invisibility.

## Deleter methodology

| Deleter case | Expected tuple result |
|---|---|
| `xmax=INVALID_TXN_ID` | VISIBLE; no-delete sentinel |
| SELF `cmax<C` | INVISIBLE |
| SELF `cmax==C` | VISIBLE |
| SELF `cmax>C` | ERROR |
| COMMITTED before snapshot | INVISIBLE |
| COMMITTED active/after snapshot | VISIBLE |
| IN_PROGRESS | VISIBLE |
| ABORTED | VISIBLE |
| RETIRED dependent | `CORRUPT_HEAP` |
| INVALID dependent | `CORRUPT_HEAP` |
| RESERVED dependent | Valid decode, then `CORRUPT_HEAP` |
| Lookup failure | Exact lower-layer error |

The explicit sentinel matrix prevents conflating `xmax == INVALID_TXN_ID` with a normal TxnId whose status result is INVALID.

## Error propagation

The methodology covers creator and deleter cases for:

- RETIRED
- INVALID
- RESERVED
- status-page I/O failure
- checksum, owner, or corruption failure
- recognizable unsupported format
- recovery/storage failure

Persisted impossible tuple metadata uses `CORRUPT_HEAP`. Runtime-only impossible states use §39.1.3’s internal-invariant path. Lower-layer errors remain unchanged, and unsupported format remains distinct from corruption.

No error may become ordinary invisibility, an ineffective delete, or silent candidate rejection.

## SELF command and causality matrices

| Field relation | Result |
|---|---|
| `cmin<C` | Creator visible |
| `cmin==C` | Creator invisible |
| `cmin>C` | Error |
| `cmax<C` | Delete effective |
| `cmax==C` | Delete ineffective to same-command rescan |
| `cmax>C` | Error |

The causality matrix covers:

- valid earlier/current inserts;
- valid current/later deletes;
- future `cmin`;
- future `cmax`;
- same-owner `cmax<cmin`.

The `cmin=C, cmax<C` fixture proves causality is checked before creator invisibility can short-circuit.

Persisted violations expect `CORRUPT_HEAP`; abstract prepublication runtime violations expect the internal-invariant path.

## Statement and UPDATE methodology

- Current-command INSERT: invisible.
- Later-command INSERT: visible.
- Current-command DELETE: old version visible.
- Later-command DELETE: old version invisible.

| Updater relation | Old visible? | New visible? | Emitted version |
|---|---:|---:|---|
| SELF current command | yes | no | old |
| SELF later command | no | yes | new |
| Other IN_PROGRESS | yes | no | old |
| COMMITTED before snapshot | no | yes | new |
| COMMITTED at/after `xmax` | yes | no | old |
| Active at capture, later COMMITTED | yes | no | old |
| ABORTED | yes | no | old |

Every row asserts exactly one emitted ordinary version. Aborted cases retain physical bytes to prove no physical undo is required.

## Isolation, retry, and recovery

READ COMMITTED coverage retains:

- one stable snapshot per attempt;
- no visibility change from a mid-attempt commit;
- fresh snapshot for the next statement;
- earlier/current SELF behavior;
- fresh snapshot plus same CommandId for legal pre-write retry;
- no same-TxnId retry after publication.

REPEATABLE READ coverage retains:

- capture on first ordinary statement;
- stable external snapshot;
- later external commits invisible;
- own later writes visible through updated CommandId;
- snapshot-isolation, not SERIALIZABLE, terminology.

Recovery coverage adds direct tuple-result oracles for:

- loser creator → ABORTED/invisible;
- loser deleter → ABORTED/ineffective;
- durable committed updater with unflushed heap/status pages;
- no use of precrash active registry or terminal cache.

## Reclamation and status ownership

Chapter 14 remains the positive proof owner for normalization, freezing, and status retirement. Verification now combines:

- positive proof that no dependent metadata remains before RETIRED;
- negative dependent-RETIRED fixture expecting `CORRUPT_HEAP`;
- missing page never implying RETIRED;
- aborted-`xmax` and frozen-creator visibility preservation;
- SQL invisibility remaining distinct from global reclamation or persisted `DEAD`.

## Heap and index recheck

Sequential scans must propagate visibility errors rather than skip malformed semantic candidates.

| Index candidate result | Required action |
|---|---|
| VISIBLE | Emit row |
| INVISIBLE | Skip candidate |
| RETIRED/INVALID/RESERVED error | Stop and propagate `CORRUPT_HEAP` |
| Lookup I/O/corruption/unsupported error | Stop and propagate exact error |
| Future SELF/causal corruption | Stop and propagate `CORRUPT_HEAP` |

Neither INVISIBLE nor ERROR may trigger ordinary predecessor traversal. Chapter 5 owns `prev`; Chapter 14 owns splicing and reclamation.

## Atomic obligation inventory

The complete 96-item inventory is recorded row-by-row in the live coverage map. Grouped without omission:

- **1–8 Structural/API:** NORMAL validation, xmin/xmax domains, canonical snapshot inputs, validation ordering, VISIBLE/INVISIBLE/ERROR domain, API freedom, causality-before-short-circuit.
- **9–27 Creator:** FROZEN; SELF `<`, `==`, `>`; canonical lookup; COMMITTED before/active/equal/above horizon; IN_PROGRESS; ABORTED; normal short-circuit; RETIRED/INVALID/RESERVED; I/O, corruption, unsupported-format, and recovery/storage propagation.
- **28–46 Deleter/causality:** no-delete sentinel; SELF `<`, `==`, `>`; COMMITTED before/active/after; IN_PROGRESS; ABORTED; RETIRED/INVALID/RESERVED; lookup failures; sentinel distinction; `cmax<cmin`; persisted/runtime error split; equality validity; creator/deleter composition.
- **47–58 Statement/update:** current/later INSERT and DELETE; SELF current/later UPDATE; IN_PROGRESS, COMMITTED-before, COMMITTED-after, active-at-capture, and ABORTED updater; exactly one emitted version.
- **59–69 Isolation/retry:** RC stable attempt, mid-attempt commit, next-statement refresh, SELF effects, pre-/post-write retry; RR capture, retention, external commits, own writes, snapshot-isolation identity.
- **70–77 Scan/recheck:** SeqScan validation, heap error propagation, B+ candidacy, visible/invisible/error index outcomes, no predecessor fallback, one statement snapshot/command.
- **78–83 Recovery:** aborted INSERT/DELETE/UPDATE, loser resolution, durable COMMIT without page force, rejection of precrash runtime state.
- **84–90 Reclamation:** aborted-xmax normalization, freezing, WAL/page-LSN mutation, RETIRED proof, dependent-RETIRED corruption, missing-page handling, invisibility versus reclamation/DEAD.
- **91–96 Cross-owner/error:** no tuple lock for reads, Chapter 11 conflict ownership, HeapPage boundary, Chapter 9 input authority, read-only transaction interaction, §39 continuation/error ownership.

Totals:

- COMPLETE: **96**
- PARTIAL: **0**
- MISSING: **0**
- CONTRADICTORY: **0**

## Final re-read answers

| # | Answer | # | Answer |
|---:|---|---:|---|
| 1 | YES | 33 | YES |
| 2 | YES | 34 | YES |
| 3 | YES | 35 | YES |
| 4 | YES | 36 | YES |
| 5 | YES | 37 | YES |
| 6 | YES | 38 | YES |
| 7 | YES | 39 | YES |
| 8 | YES | 40 | YES |
| 9 | YES | 41 | YES |
| 10 | YES | 42 | YES |
| 11 | YES | 43 | YES |
| 12 | YES | 44 | YES |
| 13 | YES | 45 | YES |
| 14 | YES | 46 | YES |
| 15 | YES | 47 | YES |
| 16 | YES | 48 | YES |
| 17 | YES | 49 | YES |
| 18 | YES | 50 | YES |
| 19 | YES | 51 | YES |
| 20 | YES | 52 | YES |
| 21 | YES | 53 | YES |
| 22 | YES | 54 | YES |
| 23 | YES | 55 | YES |
| 24 | YES | 56 | YES |
| 25 | YES | 57 | YES |
| 26 | YES | 58 | YES |
| 27 | YES | 59 | YES |
| 28 | YES | 60 | YES |
| 29 | YES | 61 | **NO — no architecture semantic rule was invented.** |
| 30 | YES | 62 | YES |
| 31 | YES | 63 | YES |
| 32 | YES | 64 | YES |

Thus all expected answers match: **YES ×60, NO, YES, YES, YES**.

## Documentation-model assessment

- Current-state leakage: none.
- DEVELOPMENT sequencing: none.
- History/devlog material: none.
- Unnecessary architecture duplication: none; rules appear only as fixture oracles with precise references.
- Time-independent: yes.
- Procedural and analytical: yes.
- Valid transaction/MVCC temporal language: preserved.
- Implementation freedom: preserved.
- Separation of concerns: preserved.
- `ARCHITECTURE.md`: unchanged.

Cross-references were validated against current headings in Chapters 5, 8, 9, 10, 11, 13, 14, 15, and §39. Existing Chapter-9, RC/RR, recovery, heap, index, reclamation, failure, and randomized/stress methodology was not removed or weakened.

## Status and repository state

- Frozen architecture semantic questions: **NONE**
- Chapter 10 follow-up verification gap: **CLOSED**
- Chapter 11 review: **NOT STARTED**
- Appendix C wording: unchanged and out of scope

Logical hunk classifications:

- A: creator invalid-status verification
- B: deleter invalid-status verification
- C: status-failure propagation
- D: SELF CommandId trichotomy
- E: same-owner causality
- F: UPDATE matrix
- G: normal visibility regression
- H: RC/RR/retry regression
- I: index/heap recheck propagation
- J: reclamation/status-retirement references
- K: matrices and 96-row coverage map
- L: precise navigation/cross-references
- M: wrapping confined to the same section

Files changed:

- `docs/VERIFICATION.md` only

Final Git state:

- Status: `M docs/VERIFICATION.md`
- Index: clean
- HEAD: `2c41e7a8c9579951f2c68e7a1168429ca2ead70b`
- `git diff --check`: passed
- No external repository changes appeared.
- No pre-existing material was modified or staged.
- No implementation, tests, builds, or benchmarks were performed.
- Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.