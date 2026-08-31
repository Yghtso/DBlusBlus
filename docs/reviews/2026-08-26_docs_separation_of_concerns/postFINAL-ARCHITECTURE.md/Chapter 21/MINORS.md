# Chapter 21 document-only-cleanup verdict

**PASS — N21-1, N21-2, N21-3, and E21-1 are CLOSED.**

Chapter 21 status:

- **ARCHITECTURE CLEAN**
- **VERIFICATION SYNCHRONIZATION PENDING**
- **NOT FULLY CLOSED**

No semantic rule changed and no new frozen semantic question was found.

## Repository and scope

Initial state:

- HEAD: `3201b4911f9427a4056a6041fafda16e7edfb854`
- Working tree: clean
- Index: clean
- Pre-existing Architecture diff: none
- D21-S1–S6 were already committed and were treated as frozen

Final state:

- HEAD unchanged: `3201b4911f9427a4056a6041fafda16e7edfb854`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- Diff: 62 insertions, 62 deletions
- Only modified file: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

No review artifact was read, modified, moved, removed, or staged.

## Sections modified

- [§21.6.1 Bound-definition handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18011)
- §21.6.2 local timeless wording
- [§21.8.1 Bound-index handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18085)
- [§21.11 INSERT bound-statement handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18288)
- §21.12 and §21.12.1 timeless default-format wording
- [§21.13 UPDATE operation planning](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18398)
- [§21.14 DELETE operation planning](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18458)
- [§21.15 RETURNING](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18474)
- [§21.17 Front-end handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18606)
- [§21.17.1 ANALYZE bound-statement and transaction boundary](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18618)
- [§21.18 SQL v1 statement surface](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18684)
- [§21.19 SQL/front-end features outside v1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18720)

## N21-1 — temporality cleanup

### Pre-cleanup project-time phrases

| Section | Phrase | Classification |
|---|---|---|
| §21.6.1 | “Initial table constraints…” | E — project-era framing |
| §21.6.1 | “Foreign keys and CHECK constraints are deferred” | E — roadmap-style scope wording |
| §21.6.2 | “assign initial ColumnIds” | B/C — object-creation scope, rephrased for precision |
| §21.6.2 | “Initial tuple schema version…” | C — durable v1 fact, rephrased timelessly |
| §21.8.1 | “Initial index keys…” | E — project-era framing |
| §21.8.1 | “Expression indexes are deferred” | E — roadmap-style wording |
| §21.12 | “first persistent default format” | E — implementation-era framing |
| §21.12.1 | “future expression-bearing blob” | E |
| §21.12.1 | “A future architecture may define…” | E |
| §21.17 | “initial parser” | E |
| §21.18 | “intended first serious SQL surface” | E |
| §21.19 heading | “Explicitly deferred…” | E |
| §21.19 | “V1 deliberately defers…” | E |
| §21.19 | “future architecture-compatible…initial engine” | E |

### Timeless rewrites

Examples:

- “Initial table constraints…” → “The v1 CREATE TABLE constraint set…”
- “Expression indexes are deferred” → “Expression indexes are outside the v1 architecture.”
- “first persistent default format” → “v1 persistent default format”
- Future default-format narration → an analytical requirement that an expression-bearing default requires a distinct format version
- “intended first serious SQL surface” → “The v1 statement surface includes”
- “Explicitly deferred…” → “SQL/front-end features outside v1”
- “future architecture-compatible…” → “not implicit requirements of the v1 SQL/front-end contract”

### Valid surviving temporal language

All surviving occurrences are semantic:

| Sections | Surviving language | Class |
|---|---|---|
| §§21.2–21.3 | first persistent/catalog write, current transaction, earlier/later command | A/B |
| §§21.5–21.10 | before/after publication, current owner/manifest, later transaction, older descriptor | B |
| §21.13 | earlier target, later demanded error, first-published-write boundary | A/B |
| §21.15 | later row failure, current-statement write | A/B |
| §21.16.1 | semantic phase, first-row encounter, later dynamic failure, condition not yet established | A |
| §21.17.1 | current SchemaVer, currently visible indexes, current command, later statement | A/B |
| §21.20 | transaction-local current-owner set | B |

Post-cleanup classification:

- A runtime/statement ordering: present and valid
- B transaction/durability ordering: present and valid
- C durable v1 scope: present and timeless
- D downstream/navigation order: present where needed
- **E project chronology: 0**

## N21-2 — binder ownership

### §21.6.1 CREATE TABLE

Removed duplicated validation list covering:

- visible-name checking
- duplicate column names
- supported types
- PK/UNIQUE/NOT NULL definition validation
- default-expression validation

Final boundary:

- §§19.4.4 and 19.20 own binding and validation.
- Chapter 21 consumes one fully bound typed schema/constraint specification.
- Chapter 21 retains ID, file, catalog, constraint/index publication, and transaction semantics.
- No persistent IDs or files are allocated during binding.

### §21.8.1 CREATE INDEX

Removed duplicated descriptions of:

- table lookup
- key-column resolution
- duplicate-key rejection
- indexable-type checking
- name binding

Chapter 21 now consumes the immutable specification from §§19.4.4 and 19.20:

- resolved TableId
- ordered ColumnIds
- uniqueness
- canonical index name

Offline build, writer exclusion, current-owner coverage, UNIQUE validation, and publication remain Chapter-21-owned.

### §21.11 INSERT

Removed duplicated binder algorithm for:

- target lookup
- duplicate-target rejection
- source binding
- coercion selection
- static NOT NULL legality

The final handoff receives from §§19.4.3 and 19.20:

- resolved target identity
- canonical full target-column order
- bound source expressions
- selected §17.8.5 coercions
- omitted-column default/typed-NULL actions

Chapter 21 retains candidate-row construction and operation semantics. Execution neither re-resolves names nor chooses another conversion.

### §21.13 UPDATE

Removed repeated validation of:

- target and SET names
- duplicate SET targets
- RHS types
- BOOLEAN WHERE
- automatic-coercion legality

The final handoff receives fully bound SET/WHERE/RETURNING expressions and resolved ColumnIds from §§19.4.3 and 19.20. All finalized-target, simultaneous-assignment, D21-S1, uniqueness, publication, affected-row, and RETURNING semantics remain local.

### §21.14 DELETE

Removed repeated target and predicate binding. Chapter 21 now consumes:

- fully bound target
- optional typed BOOLEAN predicate
- bound RETURNING metadata

Target finalization, stale handling, physical deletion semantics, index-entry retention, and RETURNING remain unchanged.

### Final handoff model

```text
Chapter 18
    parsed raw statement
        ↓
Chapter 19
    fully bound names, identities, types, coercions, and provenance
        ↓
Chapter 21
    operation semantics and publication behavior
```

Unnecessary binder duplication: **0**
Rebinding during Chapter-21 planning/execution: **prohibited and absent**

## N21-3 — parser ownership

Removed from §21.17:

- “initial parser may stop at the first syntax error”
- semicolon/end-of-input synchronization behavior
- continuation to later independent parse errors
- IDE-grade recovery statement

Final boundary:

- Chapter 18 owns lexing, parsing, request/batch framing and recovery, raw AST construction, and parse failures.
- Exact navigation points are §§18.10.1, 18.13, and 18.17.2–18.17.3.
- Chapter 21 begins only after Chapter 18 produces an independent raw statement and Chapter 19 binds it successfully.
- Chapter 21 defines no token or semicolon synchronization algorithm.
- A parse failure is not a Chapter-21 DML runtime failure.
- Statement admission, CommandId allocation, and transaction consequences remain §§9.6 and 39.1 owned.

Chapter 18 itself was unchanged.

## E21-1 — exact references

| Location | Previous reference | Final reference |
|---|---|---|
| §21.13 target sequence | “Chapter-15 target rules” | “§15.3 target rules” |
| §21.13 handoff | “Chapter 15’s update protocol” | “§15.3’s physical UPDATE version protocol” |
| §21.15 retry | “Chapter 15’s retry rule” | “§15.7 statement-attempt retry rule” |

No retry or physical-version semantic changed.

## Final ownership audits

### Binder vocabulary

| Context | Classification |
|---|---|
| §21.3 binding snapshot | B — Chapter-21 catalog-visibility consequence |
| §21.4 no side effects during binding | A/B — layer boundary |
| §§21.6.1, 21.8.1, 21.11, 21.13, 21.14 | A — precise Chapter-19 handoffs |
| §21.12 folded default persistence | B — Chapter-21 default-format operation |
| §21.15 bound RETURNING row image | B — Chapter-21 row-image ownership |
| §21.16 error categories/prerequisites | A/B — upstream category composition |
| §21.17 and §21.17.1 | A — upstream handoff |
| §21.20 parser/binder/executor invariants | A — architectural boundary |

Duplicated binder semantics: **0**

### Parser vocabulary

| Context | Classification |
|---|---|
| §21.12.1 parsing persisted default bytes | B — format decoding, not SQL parser recovery |
| §21.16 LexerError/ParserError | A — upstream error-category reference |
| §21.16.1 batch position | B — forbidden physical error-order input |
| §21.17 | A — explicit Chapter-18 delegation |
| §21.18 parser capacity | A — syntax/semantic scope boundary |
| §21.20 parser-output invariant | A — layer handoff |

Chapter-21-owned parser mechanics: **0**

### Implementation coupling

All matched terms are architecture-relevant:

- Durable allocators and physical-file states are correctness contracts.
- “Physical scan algorithm” appears only to deny semantic dependence.
- Target/RETURNING spools preserve statement atomicity and output suppression.
- Vector/batch/hash positions appear only as forbidden error-order tie-breakers.
- Physical vector position appears only in the identity-separation invariant.

Unnecessary implementation coupling: **0**

## Explicit cross-reference audit

| Source | Targets | Purpose | Status |
|---|---|---|---|
| §21.1 | Chapters 15, 20 | Storage/logical ownership | GOOD |
| §21.2 | §11.13, §39.1.4 | Gates and failed-attempt boundary | GOOD |
| §21.3 | §16.5 | Namespace classes | GOOD |
| §21.4 | §13.2.6, §16.3 | ID allocation | GOOD |
| §21.5 | §§4.7.1–4.7.7, §39.1.2 | File lifecycle/publication | GOOD |
| §21.6.1 | §§19.4.4, 19.20 | CREATE TABLE binding handoff | GOOD |
| §21.6.2 | §4.7.4, §16.5, §21.2.1 | Publication and gate lifetime | GOOD |
| §21.7 | §§16.5.4–16.5.6, §11.10 | PK catalog/runtime ownership | GOOD |
| §21.8.1 | §§19.4.4, 19.20, 14.17.1, 11.10.2, 16.5.4–16.5.5 | Bound index and publication owners | GOOD |
| §21.8.2 | §§4.7.4, 11.13, 15.5, 15.6, 21.6.2, 11.10.9 | Build/gate/terminal rules | GOOD |
| §21.9 | §§14.17.1, 11.13, 16.5.4, 16.5.6, 7.12.5, 4.7.7 | DROP coordination/dependency/retirement | GOOD |
| §21.10 | §16.10, §39.1.5 | Cache publication/failure | GOOD |
| §21.11 | §§19.4.3, 19.20, 17.8.5, 20.14 | INSERT handoff/subqueries | GOOD |
| §21.12 | §§17.10.2, 17.8.5, 17.13, 4.14.2 | Default evaluation/format | GOOD |
| §21.13 | §§19.4.3, 19.20, 17.8.5, 20.14, 15.3, 39.1, 39.3 | UPDATE handoff and operation owners | GOOD |
| §21.14 | §§19.4.3, 19.20, 20.14 | DELETE handoff/subqueries | GOOD |
| §21.15 | §§20.14.2, 15.7, 31.9, 39.1 | Subquery/retry/result publication | GOOD |
| §21.16.1 | Chapters 11, 15, 17, 20; §39.1 | Dynamic errors, scalar order, transaction consequences | GOOD |
| §21.17 | Chapter 18, §§18.10.1, 18.13, 18.17.2–18.17.3, Chapter 19, §§9.6, 39.1 | Front-end and admission handoff | GOOD |
| §21.17.1 | §§19.1, 14.17.1, 11.13, 39.1 | ANALYZE handoff/publication | GOOD |
| §21.18 | §20.14 | Subquery surface | GOOD |
| §21.20 | §§17.8.5, 4.7, 20.14, 39.1 | Summary invariants | GOOD |

All targets exist, match their canonical responsibility, and introduce no normative ownership cycle.

## Changed-paragraph document ownership

Every changed paragraph is **ARCHITECTURE**:

- v1 supported/excluded scope
- upstream parser/binder handoffs
- operation ownership boundaries
- stable persistent-format requirements
- exact cross-owner navigation

No changed paragraph belongs to DEVELOPMENT, VERIFICATION, PROJECT_STATE, devlog, or presentation/UI documentation.

## Quality assessments

- Analytical depth: **preserved**
- Terminology: **precise and synchronized**
- Normative language: **unchanged for correctness rules**
- Implementation freedom: **preserved**
- Current implementation narration: **none**
- DEVELOPMENT sequencing: **none**
- VERIFICATION procedures: **none**
- PROJECT_STATE leakage: **none**
- History/devlog material: **none**

## Semantic regressions

- D21-S1 UPDATE descriptor-wide NOT NULL: **unchanged**
- D21-S2 backing-index DROP rejection: **unchanged**
- D21-S3 current-owner CREATE INDEX view: **unchanged**
- D21-S4 deterministic DML errors: **unchanged**
- D21-S5 unordered RETURNING bags: **unchanged**
- D21-S6 dropped-name reservation: **unchanged**

Also unchanged:

- statement attempts, CommandId, retry and mandatory-abort boundaries
- INSERT mapping/default/coercion/uniqueness/affected rows/RETURNING
- UPDATE target identity, deduplication, stale handling, simultaneous SET, no-op behavior, key swaps/cycles
- DELETE target identity, stale handling, old-image RETURNING, index-entry retention
- CREATE TABLE/INDEX and DROP lifecycles
- catalog MVCC, SchemaVer, cache publication
- file lifecycle, WAL, recovery, second-crash safety
- persistent formats

Chapters 1–20 and Chapter 22+ are task-unchanged.

## Reread questions 1–82

| Questions | Result |
|---|---|
| 1–10, N21-1 | **YES** to all |
| 11–22, N21-2 | **YES** to all |
| 23–30, N21-3 | **YES** to all |
| 31–35, E21-1 | **YES** to all |
| 36–43, prohibited documentation leakage/duplication | **NO** to every “Any…?” question |
| 44–49, analytical/timeless/owner-correct completeness | **YES** to all |
| 50–55, D21-S1–S6 unchanged | **YES** |
| 56–61, B21/M21 findings remain closed | **YES** |
| 62–80, statement/DML/DDL/durability regressions | **YES** to all unchanged checks |
| 81, Chapters 18–20 unchanged | **YES** |
| 82, Chapter 22+ unchanged | **YES** |

New frozen semantic question: **NONE**

## Status closure

- N21-1: **CLOSED**
- N21-2: **CLOSED**
- N21-3: **CLOSED**
- E21-1: **CLOSED**
- B21-1–B21-4: **remain CLOSED**
- M21-1–M21-2: **remain CLOSED**
- D21-S1–D21-S6: **remain CLOSED**
- Frozen Chapter-21 semantic questions: **NONE**

Chapter-21 Architecture: **CLEAN**
Chapter-21 verification: **SYNCHRONIZATION PENDING**
Chapter 21 fully closed: **NO**

Next task: **Chapter-21 verification synchronization**
Chapter-22 direct review: **NOT STARTED**

## Hunk classifications A–S

- A: §21.6.1 timeless v1 constraint wording
- B: §21.8.1 timeless key-scope wording
- C: §21.17 removal of “initial parser” framing
- D: §21.18 canonical v1 statement surface
- E: §21.19 timeless excluded-feature scope
- F: §21.6.2 and §21.12 default-format temporality cleanup
- G: §21.6.1 Chapter-19 handoff
- H: §21.8.1 Chapter-19 handoff
- I: §21.11 INSERT handoff
- J: §21.13 UPDATE handoff
- K: §21.14 DELETE handoff
- L: §21.17 Chapter-18 delegation
- M: parse-failure/runtime-failure distinction
- N: §21.13 exact §15.3 references
- O: §21.15 exact §15.7 reference
- P: bound-statement/operation terminology synchronization
- Q: exact owner references
- R: analytical rationale preserved
- S: local Markdown wrapping only

## Final checks

- `git diff --check`: **passed**
- Index: **clean**
- HEAD unchanged
- External repository changes observed: **none**
- No staging or commit
- No build, tests, or benchmarks
- No implementation work
- No devlog or review artifact created
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
