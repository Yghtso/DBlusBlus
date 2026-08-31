# Chapter 21 semantic-integration verdict

**PASS — D21-S1 through D21-S6 integrated and CLOSED.**

Chapter 21 is now:

- **SEMANTICALLY CLEAN**
- **NOT YET DOCUMENT-CLEAN**
- **NOT FULLY CLOSED**

No new frozen semantic conflict was found.

## Repository and scope

Initial state:

- HEAD: `8aaa8c1b0604d0cedfab3fd80ced38e27d9e9986`
- Working tree: clean
- Index: clean
- Pre-existing Architecture diff: none

Final state:

- HEAD unchanged: `8aaa8c1b0604d0cedfab3fd80ced38e27d9e9986`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- Diff: 213 insertions, 16 deletions
- Only task-modified file: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

No external repository change appeared during the task. No review artifact was read, modified, moved, removed, or staged.

## Sections modified

- [§21.3 Catalog visibility during binding](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17926)
- [§21.6.2 Execution/publication](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18031)
- [§21.8.2 Offline build protocol](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18122)
- [§21.9 DROP and physical object retirement](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18195)
- [§21.13 UPDATE binding/planning](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18405)
- [§21.15 RETURNING](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18479)
- [§21.16 Error contract for semantic planning](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18529)
- New [§21.16.1 Ordinary multi-row DML runtime-error precedence](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18550)
- [§21.20 Upper semantic-layer invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18761)

No substantive edits occurred outside Chapter 21.

## D21-S1 — UPDATE NOT NULL validation

The final rule now requires every distinct finalized UPDATE target to:

1. Revalidate the old target.
2. Evaluate all SET expressions against the complete old-row image.
3. Apply bound assignment coercions.
4. Construct the complete candidate replacement row, copying unmentioned columns.
5. Validate every descriptor-declared NOT NULL column.
6. Only then proceed to immediate UNIQUE validation and publication.

Consequences:

- Both PK and non-PK NOT NULL columns are covered.
- Validation is not restricted to SET or indexed columns.
- A violation publishes no replacement tuple, old-version `xmax/cmax`, or new index entry for that target.
- The category is canonical §39.3 `ConstraintViolation`.
- An explicitly assigned NULL retains the responsible RHS SourceSpan and provenance.
- An impossible NULL copied from an architecture-valid old row remains a structural corruption/invariant condition; no synthetic SQL offset is invented.
- `SET a=b, b=a` remains simultaneous against the old row.
- `SET x=x` remains an acted-upon UPDATE and counts once.
- Existing §39.1 post-publication mandatory-abort behavior is unchanged.

Required cases A–F all produce the approved outcomes.

## D21-S2 — constraint-owned backing indexes

A constraint-owned backing index is defined by a live PRIMARY KEY or UNIQUE `sys_constraints` relationship under §16.5.6.

Standalone DROP INDEX:

- MUST reject a resolved constraint-owned backing IndexId.
- Cannot silently drop or mutate the constraint.
- Cannot delete only the index catalog row.
- Cannot begin descriptor or file retirement.
- Performs the dependency check before `LIVE -> RETIRING` or catalog deletion.
- Reports `CatalogError` at the DROP INDEX target-name construct.

Canonical backing indexes remain unnamed under §16.5.4, so they are not ordinarily name-resolvable; execution revalidation nevertheless enforces the dependency invariant.

DROP TABLE remains one operation covering the table, constraints, and dependent indexes.

Required cases A–E all produce the approved outcomes.

## D21-S3 — CREATE INDEX build view

CREATE INDEX now builds from the complete transaction-local current logical owner set at its command boundary, after writer drain.

Included:

- Current live owners committed by other transactions.
- The DDL transaction’s live rows from earlier INSERT commands.
- Its current replacement versions from earlier UPDATE commands.

Excluded:

- Aborted versions.
- Superseded historical versions.
- Rows deleted by an earlier command of the DDL transaction.
- Nonterminal versions of other transactions that cannot belong to the committing table state.
- MVCC/reclamation-only historical versions.

The build view is explicitly distinct from:

- Ordinary SQL snapshot visibility.
- Committed-only visibility.
- Raw physical tuple enumeration.

An older RR query snapshot cannot underfill the index, while ordinary RR SELECT semantics remain unchanged. CREATE UNIQUE INDEX validates over exactly the same build set.

To support:

```sql
BEGIN;
CREATE TABLE t(...);
INSERT INTO t ...;
CREATE INDEX i ON t(...);
COMMIT;
```

CREATE TABLE now acquires the new table’s existing `TableWriterGate` exclusively after allocating its fresh TableId. The transaction retains it through terminal outcome; intervening DML shared ownership is subsumed, so CREATE INDEX reuses the exclusive gate without a forbidden upgrade.

No new lock type or scan algorithm was introduced.

Required cases A–G all produce the approved outcomes.

## D21-S4 — deterministic DML runtime-error precedence

The candidate domain is restricted to ordinary row-semantic errors whose occurrence belongs to the finalized attempt and whose semantic prerequisites are satisfied.

Excluded:

- Abandoned retry attempts.
- Deduplicated-away target occurrences.
- Stale/nonfinal targets.
- Skipped or nonqualifying occurrences.
- Cancellation, resource, deadlock, storage/I/O, transaction-state, serialization, and similar dynamically owned failures.

Precedence is:

1. Smallest responsible `SourceSpan.start_byte_offset`.
2. For equal starts, shorter/more-specific SourceSpan.
3. For identical spans:

   1. scalar/subquery/final-row expression evaluation;
   2. descriptor-wide NOT NULL validation;
   3. immediate UNIQUE validation;
   4. other already-supported row constraints.

A complete tie in span, phase, category, and diagnostic origin is observationally equivalent. RID, page, source-row index, spool/vector/batch position, hash/index order, physical operator order, and scheduling are prohibited as tie-breakers.

Ordered relational input does not replace this rule with “first row encountered”; unordered input remains unordered.

Before publication that could suppress an already-determinable higher-precedence candidate or alter its §39.1 consequence, execution must establish that no such candidate exists. No prevalidation, sorting, spooling, or multi-pass implementation is mandated.

Dynamic future failures need not be predicted. Scalar child order, CommandId, retry, and post-write abort rules remain unchanged.

Required cases A–I all produce the approved outcomes.

## D21-S5 — RETURNING order

All v1 DML RETURNING results are unordered bags.

| Operation | Multiplicity | Image |
|---|---|---|
| INSERT | One occurrence per successfully inserted logical input occurrence in the final successful attempt | Final new row |
| UPDATE | One occurrence per distinct finalized target acted upon | Final new row |
| DELETE | One occurrence per distinct finalized target | Retained old row |

An ordered INSERT source does not grant RETURNING order. RID, source encounter, spool, mutation, and reclamation order are nonsemantic.

Different sequences are equivalent when their RETURNING bags are equal. This does not change affected rows, row images, errors, retry suppression, partial-prefix suppression, or explicit/autocommit result envelopes.

Required cases A–G all produce the approved outcomes.

## D21-S6 — DROP name reservation

Catalog visibility and namespace reservation are now explicitly separate.

A successful DROP may make the predecessor invisible to later commands of its owning transaction, but the canonical name remains reserved against recreation by that transaction until terminal outcome.

Therefore, within one transaction:

```sql
DROP TABLE t;
CREATE TABLE t(...);
```

and:

```sql
DROP INDEX i;
CREATE INDEX i ON ...;
```

are rejected when the CREATE reuses the reserved canonical name.

CREATE execution-time revalidation checks:

- The canonical current catalog owner view.
- Names reserved by the transaction’s earlier DROP statements.

On abort:

- The predecessor deletion is ineffective.
- No replacement identity exists.

After committed DROP:

- A later transaction may reuse the name after ordinary revalidation.
- The replacement receives fresh nonreusable IDs and FileIds.
- Old snapshots/descriptors may continue referencing the predecessor.
- Old physical retirement may remain pending independently.
- Immediate unlink is not required.

The existing separate table/index name classes are preserved.

Required cases A–F all produce the approved outcomes.

## §21.16 and §21.20 synchronization

§21.16 now distinguishes:

- Upstream binding/type errors.
- Ordinary multi-row DML row-semantic precedence.
- Constraint violations.
- Independently owned concurrency/resource/storage failures.
- §39.1 transaction consequences.

§21.20 now records concise invariants for:

- Descriptor-wide UPDATE NOT NULL validation.
- Constraint-owned backing-index protection.
- Complete transaction-local CREATE INDEX coverage.
- Physical-order-independent ordinary DML errors.
- Unordered RETURNING bags.
- Same-transaction DROP name reservation.
- Fresh identity and independent predecessor retirement after later recreation.

## Reread questions 1–129

| Questions | Result |
|---|---|
| 1–13, D21-S1 | **YES** to all |
| 14–23, D21-S2 | **YES** to all |
| 24–39, D21-S3 | **YES** to all |
| 40–63, D21-S4 | **YES** to all |
| 64–77, D21-S5 | **YES** to all |
| 78–93, D21-S6 | **YES** to all |
| 94–103, cross-decision composition | **YES** to all |
| 104, any physical row-order semantic introduced? | **NO** |
| 105, any implementation algorithm mandated? | **NO** |
| 106–120, previous/downstream owner regression | **YES** to all unchanged/preserved checks |
| 121–124, N21-1/N21-2/N21-3/E21-1 remain open | **YES** |
| 125–128, prohibited cleanup avoided | **YES** |
| 129, verification not synchronized | **YES** |

No frozen cross-owner conflict remains.

## Regression results

Unchanged:

- Statement atomicity and first-published-write boundary.
- One CommandId per admitted statement and retry reuse.
- RC pre-write retry and post-write no-retry behavior.
- Affected-row rules.
- Target deduplication, stale revalidation, and Halloween protection.
- Immediate uniqueness, same-command conflicts, key-swap failure, and key-cycle failure.
- RETURNING row images and explicit/autocommit publication envelopes.
- Catalog schemas and descriptor immutability.
- TableId/IndexId/ConstraintId/FileId nonreuse.
- Catalog-cache MVCC authority.
- DDL file states and retirement.
- WAL grammar, recovery, and second-crash rules.
- Chapters 1–20 and Chapter 22+.
- All persistent byte formats.

Persistence-format impact: **NONE**.

## Hunk classifications A–AK

- A–D: §21.13 UPDATE final-row validation, publication, error/provenance, and §21.20 invariant.
- E–H: §21.9 constraint-owned index definition, rejection, failure boundary, and error origin.
- I–N: §§21.6.2/21.8.2 current-owner build set, own-row composition, RR distinction, UNIQUE synchronization, and publication completeness.
- O–V: §21.16.1 candidate domain, SourceSpan precedence, phase precedence, tie rule, physical-order exclusion, publication protection, and dynamic-failure boundary.
- W–AA: §21.15 unordered RETURNING, operation-specific multiplicity/images, bag equivalence, and preserved result envelope.
- AB–AF: §§21.3/21.6.2/21.8.2/21.9 visibility versus reservation, recreation rejection, CREATE revalidation, terminal behavior, and retirement composition.
- AG: §21.16 error-contract synchronization.
- AH: §21.20 invariant synchronization.
- AI: Exact owner references added only where required by the six decisions.
- AJ: Concise rationale added for writer-gate composition, build-view purpose, diagnostic determinism, and identity/retirement separation.
- AK: Local Markdown wrapping only.

## Finding and status closure

- D21-S1: **CLOSED**
- D21-S2: **CLOSED**
- D21-S3: **CLOSED**
- D21-S4: **CLOSED**
- D21-S5: **CLOSED**
- D21-S6: **CLOSED**

- B21-1: **CLOSED**
- B21-2: **CLOSED**
- B21-3: **CLOSED**
- B21-4: **CLOSED**
- M21-1: **CLOSED**
- M21-2: **CLOSED**

Frozen Chapter-21 semantic questions: **NONE**

Still open and unchanged:

- N21-1
- N21-2
- N21-3
- E21-1

Next task: **Chapter-21 document-only cleanup for N21-1–N21-3 and E21-1.**

Chapter-21 verification synchronization: **NOT STARTED**
Chapter-22 direct review: **NOT STARTED**

## Final checks

- `git diff --check`: **passed**
- Index: **clean**
- Commit/staging: none
- Build: not run
- Tests: not run
- Benchmarks: not run
- Implementation work: none
- Devlog/review artifact: none created
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
