# Chapter 10 frozen-revision verdict

**COMPLETE — CHAPTER 10 ARCHITECTURE CLEAN.**

Both authorized semantic decisions and the cross-reference cleanup are integrated into [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:7523).

- Frozen semantic questions: **RESOLVED**
- Chapter 10 verification synchronization: **PENDING**
- Chapter 11 review: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**

## Git state

- Initial status: clean
- Initial index: clean
- Initial HEAD: `66bc39b76008d97e483693ad7d500ab93e7f8709`
- No pre-existing tracked or untracked state required preservation.

## D10-M1 — status/error semantics

Ordinary visibility now produces `visible` or `invisible` only for architecture-valid inputs. Required status that cannot resolve to a valid visibility outcome causes an error; lookup failures propagate unchanged.

| Creator result | Visibility outcome |
|---|---|
| `FROZEN` | Visible |
| SELF | Governed by `cmin` trichotomy |
| `COMMITTED` | Visible iff below `xmax` and absent from `snapshot.active` |
| `IN_PROGRESS` | Invisible |
| `ABORTED` | Invisible |
| `RETIRED` | `CORRUPT_HEAP` for persisted dependent metadata |
| `INVALID` | `CORRUPT_HEAP` for persisted dependent metadata |
| `RESERVED` | `CORRUPT_HEAP` for persisted dependent metadata |
| Lookup failure | Exact lower-layer failure propagated |
| Impossible other result | Corruption/internal-invariant error |

| Deleter result | Visibility outcome after creator succeeds |
|---|---|
| `INVALID_TXN_ID` sentinel | No deleter; tuple visible |
| SELF | Governed by `cmax` trichotomy |
| `COMMITTED` | Delete effective iff below `xmax` and absent from `snapshot.active` |
| `IN_PROGRESS` | Delete ineffective; old tuple visible |
| `ABORTED` | Delete ineffective; old tuple visible |
| `RETIRED` | `CORRUPT_HEAP` for persisted dependent metadata |
| `INVALID` status | `CORRUPT_HEAP`; distinct from the `xmax` sentinel |
| `RESERVED` | `CORRUPT_HEAP`; encoding remains recognized |
| Lookup failure | Exact lower-layer failure propagated |

`RETIRED` remains Chapter-14-owned and cannot substitute for COMMITTED or ABORTED. `INVALID` and `RESERVED` remain valid decoded status codes but are not terminal visibility outcomes.

Error handling reuses the existing taxonomy:

- Persisted impossible tuple metadata: `CORRUPT_HEAP`.
- Runtime-only impossible state: §39.1.3 internal-invariant failure.
- Lower-layer I/O, corruption, recovery, ownership, or format failure: propagated unchanged.
- Recognizable unsupported format: remains unsupported-format, not corruption.
- §11.10.4’s conceptual `CORRUPTION_OR_INTERNAL_ERROR` family is refined without adding an enum or prescribing an API type.

**D10-M1 status: INTEGRATED.**

## D10-M2 — SELF command causality

| SELF creator relation | Result |
|---|---|
| `cmin < command_id` | Visible |
| `cmin == command_id` | Invisible to ordinary same-command rescan |
| `cmin > command_id` | Corruption/internal-invariant error |

| SELF deleter relation | Result |
|---|---|
| `cmax < command_id` | Delete effective; tuple invisible |
| `cmax == command_id` | Delete not yet effective; tuple visible |
| `cmax > command_id` | Corruption/internal-invariant error |

The pre-visibility causality checks reject:

```text
SELF xmin and cmin > command_id
SELF xmax and cmax > command_id
SELF xmin/xmax and cmax < cmin
```

These checks occur before creator invisibility can short-circuit the decision. Persisted violations are `CORRUPT_HEAP`; unpublished runtime-only violations follow the internal-invariant path.

**D10-M2 status: INTEGRATED.**

## Evaluation order and invariants

§10.4 now explicitly requires:

1. Structural and ownership validation.
2. SELF command-causality validation.
3. Creator evaluation yielding visible, invisible, or error.
4. Deleter evaluation only after a visible creator, also yielding visible, invisible, or error.
5. Exact propagation of every creator/deleter error.

§10.6 now records:

- no guessing of transaction outcomes;
- no swallowing of status-lookup failures;
- `RETIRED`/`INVALID`/`RESERVED` as errors when status-dependent;
- future SELF command metadata as invalid;
- causal ordering for same-transaction creation and deletion.

## E1 — cross-reference cleanup

- §10.1 old navigation: “Later WAL/recovery chapters…”
- §10.1 now references:
  - §12.10.4 for abort and B+ structural shape;
  - §§13.13–13.16 for redo, loser resolution, and the no-user-DML-CLR model.

- §10.5 old navigation:
  - “later freeze rules…”
  - generic WAL/page-LSN wording;
  - broad “defined in Chapter 14.”

- §10.5 now references:
  - §§12.10–12.12 for WAL/page-LSN mutation;
  - §14.13.1 for aborted-`xmax` normalization;
  - §14.13.2 for creator freezing;
  - §§14.13.2–14.14.3 for freezing and status retirement.

No WAL, recovery, or reclamation protocol was duplicated.

**E1 status: RESOLVED.**

## Semantic regression results

All established valid behavior is unchanged:

- FROZEN, COMMITTED, IN_PROGRESS, and ABORTED creator behavior.
- No-delete, COMMITTED, IN_PROGRESS, and ABORTED deleter behavior.
- Same-command INSERT: new version invisible.
- Same-command DELETE: old version visible.
- Same-command UPDATE: old visible, new invisible.
- Later command after UPDATE: old invisible, new visible.
- Other updater:
  - IN_PROGRESS: old visible/new invisible.
  - COMMITTED before snapshot: old invisible/new visible.
  - COMMITTED after or active at capture: old visible/new invisible.
  - ABORTED: old visible/new invisible.
- READ COMMITTED, REPEATABLE READ, and retry semantics.
- Index candidate heap recheck.
- No ordinary predecessor-chain traversal.
- Chapter-14 reclamation ownership.
- Read-only transaction semantics.
- NO-FORCE, recovered terminal authority, and loser visibility.

## Technical re-read answers

| # | Answer |
|---:|---|
| 1 | YES — creator outcomes are exhaustive. |
| 2 | YES — deleter outcomes are exhaustive. |
| 3 | YES — dependent `RETIRED` fails. |
| 4 | YES — dependent `INVALID` fails. |
| 5 | YES — dependent `RESERVED` fails. |
| 6 | YES — lookup failures propagate. |
| 7 | YES — creator uses `<`, `==`, `>`. |
| 8 | YES — deleter uses `<`, `==`, `>`. |
| 9 | YES — future `cmin` fails. |
| 10 | YES — future `cmax` fails. |
| 11 | YES — `cmax < cmin` is rejected for same-owner metadata. |
| 12 | YES — same-command INSERT unchanged. |
| 13 | YES — same-command DELETE unchanged. |
| 14 | YES — same-command UPDATE unchanged. |
| 15 | YES — later-command behavior unchanged. |
| 16 | YES — RC retry unchanged. |
| 17 | YES — other-creator behavior unchanged. |
| 18 | YES — other-deleter behavior unchanged. |
| 19 | YES — aborted semantics unchanged. |
| 20 | YES — update old/new matrix unchanged. |
| 21 | YES — ordinary version-chain traversal remains absent. |
| 22 | YES — index heap recheck unchanged. |
| 23 | YES — RC/RR isolation unchanged. |
| 24 | YES — RETIRED remains Chapter-14-owned. |
| 25 | YES — continuation remains §39-owned. |
| 26 | YES — §10.1 references are precise. |
| 27 | YES — §10.5 freeze/reclamation references are precise. |
| 28 | YES — §10.5 WAL/page-LSN references are precise. |
| 29 | NO — no unrelated semantic behavior changed. |

## Documentation-model assessment

- Project chronology: none introduced.
- Current-state leakage: none.
- DEVELOPMENT sequencing: none.
- VERIFICATION procedures: none.
- PROJECT_STATE facts: none.
- Devlog/history material: none.
- Analytical rationale: preserved and strengthened at failure boundaries.
- Valid transaction-time language: preserved.
- Result: timeless, implementation-independent canonical architecture.

Compatibility remains intact with:

- Chapter 5 tuple metadata and physical version identity;
- Chapter 7 logical visibility versus page durability;
- Chapter 8 candidate RID and heap recheck;
- Chapter 9 snapshot/status ownership;
- Chapter 11 `CORRUPTION_OR_INTERNAL_ERROR` current-owner rules;
- Chapter 14 retirement proof;
- §39 failure and continuation policy.

## Follow-up verification gap

**PENDING.** A separate VERIFICATION task must cover:

1. `RETIRED`/`INVALID`/`RESERVED` and status-lookup failure.
2. Future `cmin`.
3. Future `cmax`.
4. Impossible same-transaction command ordering.
5. Complete old/new UPDATE matrix.
6. Normal creator/deleter truth-table regression.
7. Index-candidate heap-recheck error propagation.

`docs/VERIFICATION.md` was not modified.

## Diff and final repository state

Task-created hunk classifications:

- A: §10.2 creator status/error completion.
- B: §10.3.3 deleter status/error completion.
- C: §10.4 error-capable evaluation and propagation.
- D: §10.2 creator CommandId trichotomy.
- E: §10.3.2 deleter CommandId trichotomy.
- F: §10.4 same-transaction causality.
- G: §10.6 invariant summary.
- H: §10.1 precise recovery references.
- I: §10.5 precise WAL/freezing/retirement references.
- J: wrapping only within authorized hunks.

Files changed:

- `docs/ARCHITECTURE.md`

Final state:

- `git status --short`: `M docs/ARCHITECTURE.md`
- Index: clean
- HEAD: `66bc39b76008d97e483693ad7d500ab93e7f8709`
- `git diff --check`: passed
- No external repository changes appeared during the task.
- No pre-existing material was modified or staged.
- Protected documents, Chapter 11, Appendix C, source, tests, and build files are unchanged.
- No implementation work occurred.
- Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.