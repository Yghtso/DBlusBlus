# Chapter-23 document-only cleanup — PASS

M23-2 and N23-1 are closed. Chapter-23 Architecture is now clean, with Verification synchronization still pending.

## Repository state

| Check | Initial | Final |
|---|---|---|
| HEAD | `e7c7cc001c8557dce8024c4bdab9972e049f2b6b` | unchanged |
| Working tree | clean | `M docs/ARCHITECTURE.md` |
| Index | clean | clean |
| Task diff | none | 30 insertions, 6 deletions |
| `git diff --check` | — | passed |

The preceding D23 integration had already been committed. All frozen D23 semantics were preserved. Historical review artifacts were unread, unmodified, and unstaged.

## Sections modified

- [§23.1 Standard vector size and DataChunk](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19118)
- [§23.3 Vector kinds](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19207)
- [§23.4 Flat fixed-width vectors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19253)

No Chapter 26 or §39 edits occurred.

## M23-2 — Schema/column handoff

The original problem was that Chapter 23 defined `columns[]` without explicitly connecting each vector to Chapter 22’s ordered physical output schema.

The final ownership model is:

```text
Chapter 22 §22.3
    owns physical output schema and LogicalSlotId mapping
        ↓
Chapter 23
    DataChunk columns realize that ordered schema positionally
        ↓
downstream operators
    consume values through the resolved schema mapping
```

For schema `S`, Chapter 23 now states:

```text
columns.size == S.width
columns[j]   = runtime vector for physical output schema entry S[j]
```

The vector’s runtime type must agree with `S[j]`. Semantic identity remains the `LogicalSlotId` carried by `S[j]`; ordinal `j` only locates data in that chunk.

Consequences:

- Equal-valued duplicate outputs remain distinct when their schema entries have different `LogicalSlotId`s.
- `SELECT a,a`, repeated expressions, and self-join columns retain distinct identities.
- Project reordering follows the new ordered output schema.
- Derived-table remapping follows its fresh schema identities.
- DICTIONARY, selection, and borrowing affect row resolution or storage lifetime, not column identity.
- `(i,j)` locates a runtime scalar occurrence, but neither coordinate is semantic identity.
- No new ID type or mandatory per-vector `LogicalSlotId` field was introduced.

## N23-1 — Timeless wording

### Pre-cleanup inventory

| Phrase | Classification |
|---|---|
| “The initial vector representations are” | E — project chronology |
| “Later representations such as” | E — roadmap wording |
| “are deferred until measurement justifies them” | E — development/optimization sequencing |
| “not bit-packed in the initial executor” | E — current-generation narration |

### Rewrites

- “The v1 vector representations are:”
- “Representations such as SEQUENCE and RLE are outside the v1 architecture baseline.”
- “The v1 representation vocabulary is intentionally limited to the three forms above.”
- “V1 runtime BOOLEAN vectors deliberately use one byte per execution value and are not bit-packed.”

FLAT, CONSTANT, and DICTIONARY remain the exact v1 set. SEQUENCE and RLE remain outside v1. BOOLEAN representation semantics are unchanged.

### Post-cleanup temporal audit

| Section | Surviving phrase | Class | Justification |
|---|---|---|---|
| §23.1 | “uninitialized” | A | Runtime container state |
| §23.3 | “fully initialized” | A | Runtime payload/validity state |
| §23.6 | “initialized physical…” | A | Runtime resolution state |
| §23.6 | “fully initialized…” | A | Active-position invariant |
| §23.10 | “current input-consumption lifetime” | A | Borrow/lifetime interval |
| §23.13 | “reinitialize vector logical state” | A | Runtime reset operation |
| §23.14 | “initialized payload…” | A | Runtime invariant |

Project chronology count: **0**.

## Document-model audits

- Every retained Chapter-23 subsection remains Architecture.
- Current implementation narration: none.
- Development sequencing: none.
- Project State leakage: none.
- Devlog/history leakage: none.
- Verification procedure: none.
- The surviving “benchmark-configurable” phrase describes an architecture tuning constant; it is not benchmark methodology.
- Terminology remains precise and no `slot index`, `column identity`, or `vector ID` concept was introduced.
- Normative language preserves all frozen D23 requirements.
- Analytical rationale now explains why physical ordinal locates data while `LogicalSlotId` preserves identity.
- Implementations may realize the mapping positionally, through compiled mappings, or through another exact mechanism without duplicating IDs in each vector.

## Changed-reference audit

| Source | Target | Purpose | Exists/owner | Circular? | Status |
|---|---|---|---|---:|---|
| §23.1 | §22.3 | Delegate physical output-schema and `LogicalSlotId` ownership | Yes; canonical physical-plan owner | no | GOOD |

## Frozen-decision regressions

- D23-S1: unchanged; active-child selection domain and malformed-selection rules preserved.
- D23-S2: unchanged; value-stable borrowing and reset/retention rules preserved.
- D23-S3: unchanged; compact-domain, exact-alternate, `ExecutionError`, and OOM distinction preserved.
- D23-S4: unchanged; positive capacity, empty/EOS, and progress rules preserved.
- Chapter 17: task-unchanged.
- Chapter 20: bag multiplicity and occurrence identity unchanged.
- Chapter 22: task-unchanged and remains canonical schema/slot owner.
- Chapter 24+: task-unchanged.
- Chapter 26 and §39 synchronization: unchanged.
- Transactions and persistence: unchanged.

## Reread answers 1–62

1. Yes—Chapter 22 remains canonical `LogicalSlotId` owner.
2. Yes—Chapter 22 remains canonical physical output-schema owner.
3. Yes—Chapter 23 explicitly consumes that schema.
4. Yes—column `j` realizes schema entry `S[j]`.
5. Yes—vector count equals schema width.
6. Yes—runtime vector type must agree with the schema entry.
7. Yes—column ordinal is physical addressing, not semantic identity.
8. Yes—equal-valued duplicate outputs remain distinct.
9. Yes—`SELECT a,a` retains two `LogicalSlotId`s.
10. Yes—self-join columns remain distinct.
11. Yes—Project may reorder while preserving identities through its output schema.
12. Yes—derived remapping preserves its fresh identities.
13. Yes—DICTIONARY selection changes row mapping only.
14. Yes—it leaves column identity unchanged.
15. Yes—no new ID was introduced.
16. Yes—per-vector stored `LogicalSlotId` metadata is not mandated.
17. Yes—the Chapter-22-to-23 ownership direction is clear.
18. No circular normative ownership exists.
19. No “initial vector representations” chronology remains.
20. No “Later representations” roadmap remains.
21. No “deferred until measurement” wording remains.
22. No “initial executor” wording remains.
23. Yes—FLAT, CONSTANT, and DICTIONARY are the v1 representation set.
24. Yes—SEQUENCE/RLE scope is timeless.
25. Yes—they remain outside v1.
26. Yes—BOOLEAN remains byte-per-value.
27. Yes—non-bit-packed status is timeless.
28. No project chronology remains in Chapter 23.
29. No current implementation narration remains.
30. No development sequencing remains.
31. Yes—legitimate runtime temporal terms remain.
32. D23-S1 is unchanged.
33. D23-S2 is unchanged.
34. D23-S3 is unchanged.
35. D23-S4 is unchanged.
36. The active logical selection domain is unchanged.
37. Borrow stability is unchanged.
38. Large-VARCHAR policy is unchanged.
39. Positive executable capacity is unchanged.
40. Empty/EOS semantics are unchanged.
41. Chapter-26 synchronization is unchanged.
42. §39.3 synchronization is unchanged.
43. Chapter-17 semantics are unchanged.
44. Chapter-20 semantics are unchanged.
45. Chapter-22 semantics are unchanged.
46. Chapter 24+ is task-unchanged.
47. Verification is task-unchanged.
48. Development is task-unchanged.
49. Project State is task-unchanged.
50. Source, tests, and build files are task-unchanged.
51. No Verification procedure appears in Chapter 23.
52. No Project State leakage appears.
53. No devlog/history leakage appears.
54. No new frozen semantic question was found.
55. M23-2 is closed.
56. N23-1 is closed.
57. D23-S1–S4 remain closed.
58. Q23-1–Q23-4 remain closed.
59. Chapter-23 Architecture is clean.
60. Chapter-23 Verification is not yet synchronized.
61. Chapter 23 is not fully closed.
62. Chapter-24 review has not started.

## Final status

| Item | Status |
|---|---|
| M23-2 | CLOSED |
| N23-1 | CLOSED |
| D23-S1–D23-S4 | CLOSED |
| Q23-1–Q23-4 | CLOSED |
| B23-1–B23-3 / M23-1 | CLOSED |
| Frozen Chapter-23 semantic questions | NONE |
| Chapter-23 Architecture | CLEAN |
| Chapter-23 Verification | SYNCHRONIZATION PENDING |
| Chapter 23 fully closed | NO |
| Chapter-24 direct review | NOT STARTED |

Task-created hunk classes A–K are present: schema delegation, positional realization, identity clarification, duplicate/reorder preservation, timeless vector vocabulary, timeless SEQUENCE/RLE scope, timeless BOOLEAN wording, chronology removal, exact §22.3 reference, analytical rationale, and Markdown wrapping.

Only `docs/ARCHITECTURE.md` was modified. No implementation, build, test, benchmark, staging, commit, devlog, or review artifact occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.

Next task: **CHAPTER-23 VERIFICATION SYNCHRONIZATION**.
