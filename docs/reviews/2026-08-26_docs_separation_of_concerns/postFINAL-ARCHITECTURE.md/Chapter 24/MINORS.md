# Chapter-24 document cleanup verdict

**N24-1 and N24-2 are CLOSED.**

Chapter-24 Architecture is now **CLEAN**. Verification synchronization remains pending, so Chapter 24 is not yet fully closed.

## Repository state

- Initial branch: `main`
- Initial HEAD: `46c772ecb9f3bbb58a20c552a5cf88a4de4803fd`
- Initial working tree: clean
- Initial index: clean
- Prior D24 semantic integration: already committed and preserved
- Final working tree: `M docs/ARCHITECTURE.md`
- Final index: clean
- Final HEAD: unchanged
- Task diff: 22 insertions, 4 deletions
- `git diff --check`: passed
- Only `docs/ARCHITECTURE.md` was modified.
- Historical review artifacts were unread, unmodified, and unstaged.

## Sections modified

- [Chapter 24 framing](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19664)
- [§24.2 RowCollection](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19727)
- [§24.9 Spill I/O](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20107)

The RowCollection target is located in live §24.2, not §24.1. No §39 or Chapter-25+ text changed.

## N24-1 — Temporality cleanup

Pre-cleanup inventory:

| Phrase | Section | Classification |
|---|---|---|
| “The initial target block size” | §24.2 | E — project chronology |
| “The initial operator-level target” | §24.9 | E — project chronology |
| “later reconstructed” | §24.2 | A — runtime sequence |
| “currently reserved/held bytes” | §24.4 | B — live accounting state |
| “bytes currently held” | §24.5 | B — live ownership state |
| “partially initialized allocation” | §24.5 | A — allocation lifecycle |
| “hard limit is not yet reached” | §24.6 | A — runtime pressure state |
| “MUST eventually reclaim” | §24.7 | A — required resource liveness |
| “temp-manager initialization” | §24.7 | A — operational lifecycle |
| “current statement” | §§24.10–24.11 | B — transaction/runtime state |
| “eventually reclaimed” | §24.11 | A — required resource liveness |

`Concurrent` was also returned by a substring search for “current”; it is not temporal language.

Exact rewrites:

- `The initial target block size is:` → `Ordinary RowCollection blocks use a target size of:`
- `and is configuration/tuning state` → `This target is configuration/tuning state`
- `The initial operator-level target is:` → `The operator-level I/O target is:`
- `and is configurable` → `This target is configurable.`

The values and semantics remain unchanged:

- ordinary RowCollection target: `256 KiB`;
- operator I/O target: `~1 MiB I/O blocks`;
- both remain configurable resource/tuning targets;
- neither becomes a correctness or SQL limit.

Post-cleanup project chronology count: **0**.

D24-S6’s normative eventual-reclamation language remains intact.

## N24-2 — Resource policy versus SQL semantics

The new Chapter-24 framing establishes this owner direction:

- Chapter 17 owns SQL scalar domains.
- Chapter 20 owns row-count, bag multiplicity, required ordering, and `LIMIT`/`OFFSET`.
- Chapter 24 owns finite execution-resource policy.
- §39.3 owns controlled resource-error classification.

Memory budgets, accounting and hard limits, operator/block targets, spill thresholds, and runtime representation applicability govern execution feasibility. They do not redefine upstream SQL semantics or persistent tuple/database formats.

Different resource configurations may legitimately determine whether execution succeeds or reports a controlled resource failure. When legal executions both succeed, differences in budget, spill threshold, block size, spill path, retained-row representation, allocation layout, or block/run/reload layout cannot change:

- scalar values or NULL state;
- row count or bag multiplicity;
- required SQL order;
- demanded semantic errors;
- transaction effects;
- persistent database results.

RowCollection insertion/block order and spill/run/reload order remain physical. They establish no SQL order unless required by the owning operator’s canonical ordering contract.

Consequently:

- 256 KiB is not a SQL row maximum.
- Query hard limits do not define result cardinality.
- Spill thresholds do not define row eligibility.
- Runtime representation widths do not redefine `VARCHAR`.
- Chapter 24 does not redefine `LIMIT`/`OFFSET`.
- Temporary resource formats do not alter persistent formats.

The handoff is centralized once at the start of Chapter 24 rather than duplicated across subsections.

## Document-model audit

- Development sequencing: none
- Verification procedures: none
- Project-state narration: none
- Devlog/history leakage: none
- Current-implementation narration: none
- Roadmap/deferred-work narration: none
- Terminology: consistent with the frozen Chapter-24 vocabulary
- Normative language: existing D24 requirements were not weakened
- Implementation freedom: unchanged
- Analytical depth: now explains both resource ownership and why resource bounds are not SQL bounds

Changed references:

| Source | Target | Purpose | Status |
|---|---|---|---|
| Chapter 24 framing | Chapter 17 | Scalar-domain owner | GOOD |
| Chapter 24 framing | Chapter 20 | Row-count, bag, order, and LIMIT/OFFSET owner | GOOD |
| Chapter 24 framing | §39.3 | Controlled resource-failure owner | GOOD |

All targets exist, are canonical, precise, and non-circular.

## Semantic regression audit

- D24-S1 accounting universe and hard-gate atomicity: unchanged
- D24-S2 request/grant/allocation/ownership lifecycle: unchanged
- D24-S3 finite pressure progress: unchanged
- D24-S4 retained-row applicability: unchanged
- D24-S5 checked arithmetic and spill validation: unchanged
- D24-S6 namespace, cleanup, and eventual reclamation: unchanged
- M24-5 allocator-failure taxonomy: remains closed
- Chapters 17, 20–23: task-unchanged
- Chapter 25+: task-unchanged
- §39: task-unchanged
- Transaction and persistence semantics: unchanged
- New frozen semantic question: **none**

## Reread questions 1–78

- 1–7: **NO**
- 8–11: **YES**
- 12–37: **YES**
- 38–61: **YES**
- 62–66: **NO**
- 67–73: **YES**
- 74, Verification synchronized: **NO**
- 75, Chapter 24 fully closed: **NO**
- 76, Chapter 25 review started: **NO**
- 77, Phase 2 started: **NO**
- 78, Phase 2 authorized: **NO**

## Final status

- N24-1: **CLOSED**
- N24-2: **CLOSED**
- D24-S1–D24-S6: **remain CLOSED**
- Q24-1–Q24-6: **remain CLOSED**
- B24-1/B24-2: **remain CLOSED**
- M24-1–M24-5: **remain CLOSED**
- Frozen Chapter-24 semantic questions: **NONE**
- Chapter-24 Architecture: **CLEAN**
- Chapter-24 Verification: **SYNCHRONIZATION PENDING**
- Chapter 24 fully closed: **NO**
- Chapter-25 review: **NOT STARTED**

Task-created hunk classes A–L are covered by the §24.2 and §24.9 timeless rewrites plus the centralized resource-policy handoff, semantic-owner references, successful-path invariance, resource-failure qualification, persistence clarification, rationale, and Markdown wrapping.

No implementation, build, test, sanitizer, benchmark, staging, commit, devlog, or review artifact occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.

Next task: **CHAPTER-24 VERIFICATION SYNCHRONIZATION**.
