CHAPTER 25 — FULLY REVIEWED AND CLOSED.

Chapter-25 Verification is fully synchronized with the live Architecture. Only [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:16898) was modified. Architecture remains unchanged.

## Coverage

The complete [V25-R atomic ledger](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:17433) records each obligation’s Architecture section, verification procedure, independent oracle, reuse reference, and status.

| Classification | Count |
|---|---:|
| TOTAL ATOMIC | 183 |
| CORRECTNESS-RELEVANT | 181 |
| COMPLETE | 181 |
| PARTIAL | 0 |
| MISSING | 0 |
| CONTRADICTORY | 0 |
| N/A | 2 |

The two N/A rows are explicitly justified:

- Named scalar-function argument, strictness, and volatility execution: the v1 registry has no signatures.
- Malformed function-local state of an otherwise valid named scalar function: no such valid state exists. Rejection of a purported executable named function is separately COMPLETE.

These are methodology-coverage counts, not claims that implementation tests ran.

## Sections and methodology

Added the coherent V25-A–R family and made narrowly scoped corrections to the existing Expression Execution Tests.

| Family | Methodology and matrix |
|---|---|
| A | Independent scalar/tree oracles; complete executable-form inventory; batch dispatch and immutable-state checks |
| B | Generic parent/child demand, active domains, mask independence, empty demand, speculative nonvisibility |
| C | AND/OR/CASE/IN demand matrix and recursive mask composition |
| D | LogicalSlotId → physical schema → DataChunk mapping matrix |
| E | Result-occurrence, multiplicity, placement, and empty-domain matrix |
| F | FLAT/CONSTANT/DICTIONARY substitutability |
| G | NULL validity and poisoned-payload checks |
| H | Arithmetic, FLOAT, comparison, cast, VARCHAR, and folding equivalence |
| I | Ordinary candidate-domain and excluded-owner matrix |
| J | Source/cause preorder, cause matrix, equivalence, deterministic reduction, early stopping |
| K | DML provenance, candidate preservation, phase, RETURNING, and retry matrix |
| L | Borrow graph, lifetime, reset/reuse, and computed-VARCHAR matrix |
| M | Symbolic large values, exact extents, accounting, and resource matrix |
| N | Invalid-state classification and safe-rejection matrix |
| O | Successful/failed expression publication and prior cursor-chunk matrix |
| P | Width/layout/worker/SIMD determinism matrix; reuse and persistence-negative registry |
| Q | Thirteen cross-chapter handoffs with exact reusable procedures and independent oracles |
| R | Complete atomic ledger, totals, N/A justifications, and stale-rule audit |

All thirteen requested substantive matrices are included, alongside the atomic ledger.

## Principal verification results

Scalar and demand methodology uses independently authored Chapter-17 arithmetic, binary64 rounding, byte-string, calendar, cast, and 3VL models. Production kernels are never their own oracle. Supported forms include NULL predicates and specialized subquery handoffs; no named functions or unsupported operators were invented.

Recursive demand checks prove that descendants cannot restore ancestor-excluded occurrences or drop required work for vector convenience. Empty demand produces no per-row evaluation. Speculative undemanded values and errors remain invisible. NULL strictness suppresses the non-NULL payload operation—not errors from evaluating another required child.

Slot and occurrence procedures cover reordered `C,A,B` physical schemas, self-joins, equal values, repeated outputs, derived remaps, and temporary vectors. Physical ordinals remain locators. Successful ordinary evaluation produces one corresponding result per demanded occurrence; CONSTANT payload sharing and repeated DICTIONARY indices do not collapse multiplicity. Filter removal and Project placement remain operator-owned.

D25-S1 verification independently applies:

1. Smallest responsible span start.
2. Shorter represented span for equal starts.
3. Canonical semantic occurrence order for identical spans.
4. `INVALID_CAST`, `NUMERIC_OVERFLOW`, `DIVISION_BY_ZERO`, `INVALID_DATE`, `INVALID_TIMESTAMP`.

Concrete same-origin cases verify overflow beating zero division, and invalid numeric text beating numeric cast overflow. Equivalence compares all frozen diagnostic fields, not prose, offending-value rendering, or candidate addresses. Project, Filter, dictionary repetition, worker merges, SIMD completion permutations, and early-stop cases use the same independent minimum-class oracle.

DML verification preserves provenance, category/cause, expression-phase membership, eligibility, and every candidate needed by D21-S4. RETURNING remains DML. The separation fixture uses abandoned-versus-finalized attempt eligibility, plus multiple eligible candidates delivered in adverse order and existing phase-ranking fixtures. It does not invent an opposite DML cause winner where the two rules legitimately agree.

Borrow verification covers payload, validity, selection, representation metadata, dictionary relationships, StringRef metadata/bytes, aliases, and consumer intervals. Synchronous zero-copy remains conforming; retention requires stable ownership without mandating deep copy.

Large-value verification uses symbolic lengths around and above `UINT32_MAX`, avoiding multi-GB allocations. It distinguishes exact alternate representations, unsupported representation (`ExecutionError`), supported allocation denial (`OutOfMemory`), and cancellation. Exact-before-use arithmetic and continuously accounted ownership cover vectors, masks, output bytes, and scratch. No universal resource-versus-semantic precedence was added.

Invalid-state verification covers child count, input/output types, missing kernels, slot mapping, selections, borrows, validity, initialization, and unsupported executable function state. These remain internal failures, not scalar errors. Safe rejection precedes unsafe access, stale publication, arbitrary mutation, or persistent effects.

Publication verification independently proves:

- Successful demanded output is fully initialized.
- Failed invocations cannot publish partial or stale results as success.
- Earlier returned cursor chunks are not retroactively retracted.
- A returned prefix is not successful query completion.
- Existing cursor lifetimes and DML result envelopes remain unchanged.

## Audits and regression

Corrected Chapter-25-relevant stale methodology:

- Blanket `CastError` expectations for casts, including numeric overflow.
- Unsupported volatile-function execution fixtures.
- A three-by-three formulation incorrectly applied to unary NOT.
- Unqualified error invariance that did not distinguish legitimate resource-feasibility differences.

The remaining demand, slot, multiplicity, borrowing, large-value, and invalid-state methodology was consistent or received explicit V25 coverage.

Document checks found:

- Duplicate ledger IDs: 0.
- Malformed ledger rows: 0.
- Matrix column mismatches: 0 after correction.
- Project chronology/current-state narration: 0.
- Development sequencing/history leakage: 0.
- New Architecture semantics: none.

Surviving temporal terminology describes invocation initialization, DML phases, borrow intervals, or prior/subsequent cursor operations. References retain canonical owner direction.

D25-S1, D20-B1/B2, D21-S4/S5, slot identity, vector/borrow contracts, resource classification, cursor publication, transaction consequences, and persistence semantics remain unchanged.

## Reread answers 1–186

Each range below answers every numbered question in that range.

| Questions | Answer |
|---|---|
| 1–17: demand | YES, all applicable |
| 18–34: slots/results | YES, all applicable |
| 35–51: scalar equivalence | YES, all applicable |
| 52–85: D25-S1 | YES |
| 86–97: DML handoff | YES |
| 98–122: borrowing/resources | YES |
| 123–145: invalid state/publication | YES; Q132 preserves the applicability guard—valid named-function state is N/A |
| 146–167: determinism/document model | YES |
| 168–172: ledger and zero unresolved coverage | YES |
| 173: frozen semantic question | NO |
| 174–183: coverage and closure | YES |
| 184–186: Chapter 26 / Phase 2 started or authorized | NO |

## Repository and closure

| Item | Initial | Final |
|---|---|---|
| Working tree | Clean | `M docs/VERIFICATION.md` |
| Index | Clean | Clean |
| HEAD | `5be005645b5e68467bbe35ceea28ebc986710f2c` | Unchanged |
| Architecture diff | None | None |

Task diff: **760 insertions, 7 deletions**, only in Verification. Hunk classifications cover A–AI: methodology, matrices, reuse, scoped stale corrections, ledger, and document-quality checks.

`git diff --check` passes. No external changes were observed. Historical review artifacts remained unread, unmodified, unmoved, and unstaged.

Final status:

- Architecture: CLEAN.
- Verification: FULLY SYNCHRONIZED.
- D25-S1 and M25-1–M25-4 verification: COMPLETE.
- N25-1–N25-3 correctness-relevant coverage: COMPLETE.
- D25-S1, Q25-1, B25-1, and all seven cleanup findings remain CLOSED.
- Frozen Chapter-25 semantic questions: NONE.
- Chapter 26 review: NOT STARTED.

Next task: CHAPTER 26 DIRECT READ-ONLY ARCHITECTURE REVIEW—not performed here.

No implementation, build, tests, sanitizers, benchmarks, staging, commit, devlog, or review artifact occurred. Phase 2 remains NOT STARTED / NOT AUTHORIZED.