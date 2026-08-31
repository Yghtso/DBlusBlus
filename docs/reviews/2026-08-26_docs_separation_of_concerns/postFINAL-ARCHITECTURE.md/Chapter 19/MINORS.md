# Chapter 19 document-only cleanup — PASS

Chapter 19 is now architecturally clean. F19-N1, F19-N2, and F19-N3 are closed without changing frozen binding semantics.

Modified file: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15419)

## Repository state

| Item | Initial | Final |
|---|---|---|
| HEAD | `fd2851e6fe63f7754eccc207222bced87436e776` | unchanged |
| Working tree | clean | `M docs/ARCHITECTURE.md` |
| Index | clean | clean |
| Architecture diff | empty | 103 insertions, 89 deletions |
| `git diff --check` | — | PASS |

The prior semantic integration was already committed at task start. No pre-existing working-tree changes required preservation. No review artifact was read, modified, moved, or staged.

Boundary hashes prove Chapters 1–18 and Chapter 20 onward are byte-identical to HEAD:

| Boundary | HEAD hash | Final hash | Result |
|---|---|---|---|
| Before Chapter 19 | `df88922d…1288` | `df88922d…1288` | unchanged |
| Chapter 20 onward | `ea0792ca…fa8` | `ea0792ca…fa8` | unchanged |

## Sections changed

§§19.1, 19.2, 19.2.1, 19.2.2, 19.3, 19.4.3, 19.4.4, 19.5–19.10, 19.13, 19.14, 19.17, 19.19, and 19.20.1–19.20.3.

## F19-N1 — timelessness

### Project-time phrase inventory

| Original wording/concept | Classification | Result |
|---|---|---|
| “future architecture” | Project roadmap | Removed |
| “Logical planning later assigns…” | Downstream navigation | Replaced with exact §20.2 ownership |
| “Initial kinds include” | Project-time framing | Rewritten as “The v1 bound-expression kinds include” |
| “later diagnostics” | Downstream navigation | Rewritten as “downstream diagnostics” |
| “later vectorized evaluation” | Downstream navigation | Replaced with §25.1 handoff |
| “retained for later versions” | Project roadmap | Replaced with timeless descriptor-role wording |
| “When later registered” | Project roadmap | Replaced with canonical IMMUTABLE-function rule |
| “Initial aggregate expressions” | Project-time framing | Rewritten as “The v1 aggregate expressions” |
| “initial … signatures” | Project-time framing | Rewritten as the §29.3.2 signatures |
| “different return type later” | Pipeline wording | Rewritten as execution using the same return type |
| “later planning/execution” | Vague owner navigation | Replaced by §§20.11 and 30.1 |
| “Later execution may choose…” | Implementation/pipeline wording | Abstracted to implementation-defined physical strategy |
| “deferred from the initial parser target” | Project chronology | Replaced with timeless v1 grammar exclusion |
| “future parameter typing” | Project roadmap | Removed |
| “later cardinality legality” | Validation order | Rewritten as “cardinality legality” |
| “later call TypeError” | Prerequisite order | Rewritten without temporal phrasing |
| Future-parameter invariant | Project roadmap | Removed |

### Surviving temporal words

Every surviving occurrence is semantically necessary:

| Section | Occurrence | Classification |
|---|---|---|
| §19.2 | later relation occurrence | Source order |
| §19.2.1 | earlier/later FROM sibling | Source-order visibility |
| §19.2.1 | current right/later join relation | Join-chain visibility |
| §19.4.1 | current query block | Scope identity |
| §19.4.2 | current block | Scope identity |
| §19.11 | current query block | Aggregate ownership |
| §19.20.3 | current-right semantics | Join-scope invariant |

No project chronology, roadmap language, current implementation narration, or unjustified temporal language remains.

## F19-N2 — implementation guidance

The following guidance was removed or abstracted:

- “Arena ownership or shared immutable nodes are both architecture-compatible.”
- Advice against independently reference-counted per-node heap allocation unless measurement justified it.
- Concrete IN-list algorithm suggestions based on list size.
- Implementation-oriented wording about type rules being scattered through AST classes.

The final §19.7 contract now states only that:

- bound expressions are immutable;
- rewrites may create new expressions or structurally share immutable children;
- expression lifetime is query/plan scoped;
- allocation, ownership, sharing, and storage representation are implementation-defined;
- §18.14 backing-lifetime and §19.2 BindingId identity rules must hold;
- pointer and allocation identity cannot become observable query semantics.

Arena, ref-count, allocation-churn, and measurement advice is absent. Such engineering guidance may belong in DEVELOPMENT in a separately authorized task; it was not moved here.

### Implementation-coupling audit

Surviving implementation-related language is correctness-significant:

- BindingId representation/width/allocation are implementation-defined.
- §25.1 defines the required execution handoff.
- Runtime implementation IDs distinguish dispatch identity from persistent state.
- Container iteration, pointer addresses, and allocation layout are excluded from error selection.
- Physical IN-list strategy is explicitly implementation-defined but must preserve §17.7.3.
- Implementation-sized LIMIT maxima are excluded from SQL semantics.

No arena, reference-count, allocator, visitor, map, multi-pass, benchmark, optimization, or source-layout mandate remains.

## F19-N3 — ownership and references

### Pre-cleanup deficiencies resolved

| Deficiency | Result |
|---|---|
| Broad “Chapter 16” descriptor reference | §§16.6, 16.10, and 21.3 |
| Broad Chapters 16/18 identifier reference | §§16.2 and 18.4 |
| Broad Chapter-18 JOIN reference | §18.11 |
| Missing INSERT target-mapping owner | §21.11 |
| Broad Chapter-18 DML-alias reference | §18.10.3 |
| Broad Chapter-15/21 RETURNING reference | §21.15 |
| Broad §21.6–§21.7 constraint reference | §§21.6.1 and 21.7 |
| Broad CREATE INDEX owners | §§21.8.1, 21.8.2, and 11.10 |
| Missing wildcard presentation-order owner | §16.8 |
| Broad Chapter-20 output-slot handoff | §20.2 |
| Broad CAST provenance reference | §§18.8 and 18.13 |
| Broad §§17.6–17.7 operator reference | §§17.6 and 17.7.1 |
| Missing exact TypeResolver owner | §17.10.1 |
| Broad generic-call syntax owner | §18.12.4 |
| Broad aggregate descriptor reference | §§29.3 and 29.3.2 |
| Broad ORDER BY provenance owner | §18.13 |
| Vague sorting handoff | §§20.11 and 30.1 |
| Broad orderability owner | §17.7.1 |
| Broad LIMIT coercion/folding references | §§17.8.5 and 17.10.2 |
| Broad unknown-type parse boundary | §18.12.5 |
| Broad error-category wording | §§39.2 and 39.3 |
| Broad Chapter-21 row-image invariant | §21.15 |

Aggregate ordinal already referenced §29.3.7 and was verified correct. DISTINCT already referenced §20.10. DEFAULT already referenced §21.12. Subquery references to §§20.14 and 20.14.3 were preserved.

### Complete final cross-reference table

Repeated references within one section are consolidated below; no explicit Chapter-19 reference is omitted.

| Source | Targets | Purpose | Exists/owner/precision | Status |
|---|---|---|---|---|
| §19.1 | §§16.6, 16.10, 21.3 | Descriptor and catalog visibility | Yes / canonical / exact | GOOD |
| §19.2 | §§16.2, 18.4 | Canonical identifier bytes | Yes / canonical / exact | GOOD |
| §19.2.1 | §§19.18, 18.11, 19.14 | Correlation, JOIN shape, LIMIT scope | Yes / canonical / exact | GOOD |
| §19.2.2 | §19.18 | Parent-scope diagnostic use | Yes / local owner / exact | GOOD |
| §19.3 | §20.2 | LogicalSlotId handoff | Yes / canonical / exact | GOOD |
| §§19.4.1–2 | §19.18 | Outer diagnostic lookup | Yes / local owner / exact | GOOD |
| §19.4.3 | §§21.11, 17.8.5, 18.10.3, 21.13–21.15, 19.10 | DML mapping, aliases, row images, aggregates | Yes / canonical / exact | GOOD |
| §19.4.4 | §§21.6.1, 21.7, 21.8.1–2, 21.3, 18.13, 11.10, 21.9 | DDL binding/publication boundaries | Yes / canonical / exact | GOOD |
| §19.5 | §§16.8, 19.2, 18.14, 20.14.3, 20.2 | Wildcards, lifetime, derived outputs, slots | Yes / canonical / exact | GOOD |
| §19.6 | §§18.8, 18.13, 18.14, 25.1 | SourceSpan, AST, lifetime, execution handoff | Yes / canonical / exact | GOOD |
| §19.7 | §§18.14, 19.2 | Lifetime and BindingId constraints | Yes / canonical / exact | GOOD |
| §19.8 | §§17.6, 17.7.1, 17.10.1 | Operators, comparisons, TypeResolver | Yes / canonical / exact | GOOD |
| §19.9 | §§17.9.3, 17.10.1, 18.12.4, 29.3.2, 19.10, 21.12 | Calls, errors, aggregates, defaults | Yes / canonical / exact | GOOD |
| §19.10 | §§29.3, 29.3.2, 29.3.7 | Aggregate registry, signatures, ordinal | Yes / canonical / exact | GOOD |
| §19.11 | §20.9 | Runtime grouping equality boundary | Yes / canonical / exact | GOOD |
| §19.13 | §§19.5, 18.13, 20.11, 30.1, 17.7.1 | Alias, ordinal provenance, sorting, orderability | Yes / canonical / exact | GOOD |
| §19.14 | §§17.8.5, 19.20, 20.12, 17.10.2, 17.6, 17.8 | Coercion, precedence, logical LIMIT, folding/errors | Yes / canonical / exact | GOOD |
| §19.15 | §20.10 | DISTINCT logical owner | Yes / canonical / exact | GOOD |
| §19.16 | §§17.9.1, 17.7.3 | CASE typing/evaluation | Yes / canonical / exact | GOOD |
| §19.17 | §§17.9.2, 17.7.3 | IN typing/evaluation | Yes / canonical / exact | GOOD |
| §19.18 | §§20.14, 20.14.3 | Subquery and derived-table semantics | Yes / canonical / exact | GOOD |
| §19.20.1 | §§39.2, 39.3, 21.12, 18.12.5, 18.17 | Errors, defaults, parse boundary, resources | Yes / canonical / exact | GOOD |
| §19.20.3 | §§17.10.2, 20.10, 19.4.3–4, 21.15, 17.2–17.10, 39.1 | Integrated invariants and handoffs | Yes / canonical or precise integration range | GOOD |

No cross-reference substitutes for a binder-owned semantic rule, and no downstream semantics were duplicated.

## Document ownership audit

All changed paragraphs remain ARCHITECTURE material:

| Sections | Architectural responsibility |
|---|---|
| §§19.1–19.3 | Binder boundary, identity, scope, downstream slot handoff |
| §§19.4.3–19.4.4 | DML/DDL binding namespaces and exact ownership boundaries |
| §§19.5–19.10 | Bound metadata, representation contract, type/call/aggregate resolution |
| §§19.13–19.17 | Clause binding and execution handoffs |
| §19.19 | Closed v1 grammar/binder scope |
| §19.20 | Error ownership, deterministic precedence, invariants |

There is no DEVELOPMENT sequencing, VERIFICATION procedure, PROJECT_STATE narration, devlog/history, or UI-only material.

## Semantic regression assessment

All frozen semantics remain unchanged:

- Raw AST and Chapter-18 grammar
- BindingId whole-statement identity and lifetime
- qualifier uniqueness, alias hiding, and outer shadowing
- ambiguity behavior and wildcard order
- output-name derivation and duplicate-name legality
- SELECT alias and ORDER BY resolution
- GROUP BY alias/ordinal exclusion
- aggregate placement, HAVING, global grouping, and structural equality
- ORDER BY ordinal syntax/range/priority
- LIMIT/OFFSET eligibility, typing, mandatory folding, and final validation
- DML namespaces and RETURNING row images
- DDL declaration/index/drop validation
- generic-call TypeError semantics
- aggregate ordinal
- binder error categories and deterministic precedence
- implicit/explicit cast provenance
- catalog snapshot semantics
- Chapters 17, 18, 20, 21, and 29
- §39 transaction/error semantics
- persistence and recovery formats

## Acceptance/reread answers 1–79

- Questions **1–68:** YES individually.
- Questions **69–74:** NO individually—no unjustified chronology, current-state narration, DEVELOPMENT guidance, verification recipe, PROJECT_STATE leakage, or history/devlog material remains.
- Questions **75–79:** YES individually—Chapter 19 is analytical, timeless, independently canonical, implementation-independent, and precise in ownership.

No item required an N/A classification.

## Finding and decision status

| Item | Status |
|---|---|
| F19-N1 | CLOSED |
| F19-N2 | CLOSED |
| F19-N3 | CLOSED |
| F19-N4 | remains CLOSED |
| F19-B1–B7 | remain CLOSED |
| F19-M1–M5 | remain CLOSED |
| D19-S1–S18 | remain CLOSED |
| D19-C1 | remains CLOSED |
| D19-C2 | remains CLOSED |
| New frozen semantic questions | NONE |

Chapter-19 architecture status: **CLEAN**.

Chapter-19 verification: **SYNCHRONIZATION PENDING**.

Chapter 19 fully closed: **NO**.

Chapter-20 direct review: **NOT STARTED**.

## Task-created hunk classification A–U

| Class | Result |
|---|---|
| A | BindingId/parent-scope timelessness cleaned |
| B | Bound-expression-kind wording cleaned |
| C | Function-registry roadmap wording removed |
| D | Aggregate wording made timeless |
| E | Parameters expressed as v1 grammar exclusion |
| F | Temporal invariant/navigation wording cleaned |
| G | Other vague temporal handoffs made precise |
| H | §19.7 performance guidance removed |
| I | Representation freedom stated analytically |
| J | AST-class and IN-strategy coupling abstracted |
| K | §29.3.7 aggregate-ordinal reference verified; no edit needed |
| L | §17.7.1 orderability reference added |
| M | §20.10 DISTINCT reference verified; no edit needed |
| N | Catalog/descriptor references made exact |
| O | DML/RETURNING references made exact |
| P | DDL/default/index/drop references made exact or verified |
| Q | §§39.1–39.3 ownership made precise |
| R | §§17.9.3 and 17.10.2 references preserved/refined |
| S | §§20.14 and 20.14.3 references verified unchanged |
| T | Invariants and terminology synchronized |
| U | Local wrapping only |

Only `docs/ARCHITECTURE.md` was task-modified.

No external repository change was detected. No staging or commit occurred. No build, test, benchmark, implementation work, devlog, or review artifact was created.

Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
