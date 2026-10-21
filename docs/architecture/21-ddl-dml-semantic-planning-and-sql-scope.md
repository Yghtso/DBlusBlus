# 21. DDL/DML Semantic Planning and SQL Scope

## 21.1 Scope

This chapter owns the upper semantic integration for schema-changing DDL, transactional catalog visibility, catalog-object identity publication, offline index construction, DROP/object retirement, DML binding/planning, default semantics, RETURNING, and SQL scope.

Storage mutation ordering remains Chapter 15's responsibility. Logical operator semantics remain Chapter 20's responsibility.

## 21.2 Conservative DDL concurrency model

The architecture serializes schema-changing DDL with one database-wide `SchemaLock` or equivalent exclusive catalog-DDL mutex.

It is a transaction-owned gate with the exact mode, reentrancy, wait-for
membership, and release rules in §11.13. It is not table-scoped. A transaction
that requests it after retaining a writer or statistics-publication gate does
not rely on per-statement order or timeout: the request is either one of
§11.13's two proactive rejections or a represented unified-graph wait.

A transaction that successfully completes schema-changing DDL or publishes its first transaction-owned catalog mutation holds this exclusivity through its terminal publication/abort boundary. A recoverable failed attempt that remains pre-catalog under §39.1.4 transfers every physical survivor to orphan cleanup and releases attempt-only DDL exclusivity before leaving the transaction `ACTIVE`.

Ordinary read-only queries do not hold this lock for their execution lifetime.

### 21.2.1 Target-table writer gate

DDL that must observe or retire a stable physical table state additionally uses a per-table writer gate. DML that performs persistent writes to table `T` acquires: `TableWriterGate(T) shared` before the first persistent write to `T` and holds it until transaction end.

CREATE INDEX / DROP TABLE / DROP INDEX on `T` acquire: `TableWriterGate(T) exclusive` and wait for existing table writers to reach terminal outcome before proceeding.

Reads do not require this writer gate. The gate is a DDL coordination mechanism, not a replacement for tuple-write/unique-key locking. No SchemaLock/TableWriterGate wait occurs while holding heap/B+ page latches.

DDL lock order is:

```text
SchemaLock
    -> TableWriterGate exclusive when required
    -> optional private-file construction with no short latch retained across
       a later transaction-gate wait
    -> MANIFEST_CHANGE when the table/index manifest changes or retires
    -> live-object/catalog publication locks/latches
```

The architecture does not support lock upgrade from a transaction's already-held shared TableWriterGate to exclusive DDL ownership. If an explicit transaction has already performed persistent DML on the target table, a later schema-changing DDL statement requiring that exclusive gate is rejected.

This deliberately coarse baseline prevents an offline index build from missing concurrent writes and avoids an implicit lock-upgrade deadlock protocol.

The rejection is target-specific. `UPDATE A` followed by `CREATE TABLE B` is
otherwise legal and deadlock-managed; `UPDATE A` followed by `CREATE INDEX A`
is rejected before waiting. DDL acquired first may be followed by DML: its own
exclusive gate subsumes a shared request on the same table, and a different
table's shared request follows §11.13. Several supported DDL statements reuse
the global SchemaLock and acquire each new target gate normally.

## 21.3 Catalog visibility during binding

Catalog rows participate in normal MVCC.

Binding uses the transaction's normal catalog snapshot:

```text
READ COMMITTED:
    statement snapshot

REPEATABLE READ:
    transaction snapshot
```

Therefore uncommitted DDL from another transaction is invisible, a long-running REPEATABLE READ transaction does not suddenly resolve a newly committed object, and the current transaction may observe its own completed earlier DDL statement through ordinary self/CommandId visibility.

Catalog visibility and namespace reservation are distinct. A successful DROP
may make an object invisible to later commands of the dropping transaction
through catalog MVCC while its canonical name remains reserved by that
transaction. The architecture retains that reservation until the transaction reaches a
terminal outcome, so the dropping transaction cannot assign the name to a
replacement object. The reservation applies within the existing §16.5 name
class: table and named-index names remain separate classes.

The catalog cache cannot override this rule. A cached descriptor is usable only if it is visible to the binding snapshot.

## 21.4 Durable catalog-object IDs

The durable catalog-object allocator is defined in §13.2.6.

The architecture uses one durable uint64 catalog-object ID namespace for:

```text
TableId
IndexId
ConstraintId
```

Allocation occurs during DDL execution, not during syntax parsing or side-effect-free binding. Binding may create an immutable/provisional descriptor specification, but it does not perform durable identity allocation or file creation merely to resolve a statement.

The allocation sequence is:

```text
acquire relevant DDL exclusivity
durably reserve/advance next_catalog_object_id
receive object ID
only then place that ID in persistent catalog/file state
```

Crash gaps and IDs consumed by aborted DDL are acceptable. Reuse is forbidden. ColumnIds follow the table-local rule in §16.3.

## 21.5 DDL private physical resources

A physical heap/FSM/B+ file created by uncommitted DDL follows the exact private/final namespace lifecycle in §4.7.1–§4.7.7.

Before durable final-name publication it is **private/unpublished** under its exact `pending/` basename. After durable no-replace rename but before transaction commit it is `FINAL_DURABLE_UNCOMMITTED`; filesystem visibility still does not make it a committed catalog object.

Other transactions cannot discover it through committed catalog metadata.

If CREATE DDL aborts:

```text
catalog rows become ABORTED/invisible
allocated object IDs/FileIds remain consumed
private or final-uncommitted physical files become orphan-retirement candidates
```

The architecture need not physically undo their bytes. Orphan classification and durable unlink use §4.7.6–§4.7.7 only after no live in-process owner can reference the file. An object file is never considered committed merely because it exists on disk.

Consumed nonreusable TableId/IndexId/ConstraintId/FileId values, private-file initialization/build WAL, and durable private/final namespace entries remain allocator/DDL-cleanup artifacts rather than transaction-owned logical statement writes until catalog mutation publishes. A recoverable failure before that catalog boundary may leave the explicit transaction `ACTIVE` only after every survivor has deterministic orphan-cleanup ownership and attempt-only DDL locks are released. IDs remain consumed. Publishing a CREATE catalog row or DROP/catalog deletion marker crosses §39.1.2's boundary; any later statement failure mandates ABORT.

Before a DDL transaction may enter its terminal COMMIT sequence, every physical file referenced by that transaction's new catalog state MUST have completed the §4.7.4 final-name publication barrier. A namespace-sync failure therefore occurs before durable terminal commit and cannot be reported as successful CREATE publication.

## 21.6 CREATE TABLE

### 21.6.1 Bound-definition handoff

CREATE TABLE execution consumes the fully bound and validated definition from
§§19.4.4 and 19.20. The bound statement supplies the canonical table name and
one typed schema/constraint specification; Chapter 21 does not resolve names,
types, constraint members, or default expressions again.

The CREATE TABLE constraint set is PRIMARY KEY, UNIQUE, and NOT NULL.
Foreign keys and CHECK constraints are outside the architecture.

Binding allocates no persistent object or FileId.

### 21.6.2 Execution/publication

Under SchemaLock:

```text
1. revalidate table-name availability against the canonical current catalog
   owner view and names reserved by this transaction's earlier DROP statements
2. allocate TableId
3. acquire TableWriterGate(TableId) exclusive for the newly identified table
4. allocate heap FileId and FSM FileId
5. create/initialize private heap/FSM files
6. assign ColumnIds 1..N for tuple schema version 1
7. build any required primary/unique index objects as private files through
   their DDL protocol
8. complete §4.7.4 durable final-name publication for the entire required
   heap/FSM/index physical file set
9. only then install transaction-owned MVCC catalog rows/descriptors that
   name those final physical files using the exact §16.5 schema-v1 rows
10. finish the DDL statement while retaining DDL exclusivity until the owning transaction is terminal
11. before COMMIT, reverify that the transaction's required file set remains
    durably final-name-published
12. if/when the owning transaction reaches terminal COMMITTED publication:
       publish new committed descriptor-cache/name entries
13. on ABORT:
       keep catalog rows invisible and retire private/final-uncommitted files later
14. release DDL exclusivity only at that terminal boundary
```

No pre-existing transaction can own the fresh TableId's writer gate. Retaining
its exclusive ownership permits the creating transaction to perform DML and a
later CREATE INDEX on that transaction-local table without a lock upgrade:
the exclusive gate subsumes the DML shared request and is reused by the index
build. Its terminal-lifetime rule remains §21.2.1.

The created table's tuple schema version is `1`. If the transaction aborts, its catalog rows remain invisible and created files become orphan-retirement candidates.

In autocommit mode the statement-owning transaction normally reaches COMMIT immediately after successful statement completion. In an explicit transaction, the object remains transaction-local until the later user COMMIT and the conservative SchemaLock remains held for that interval.

## 21.7 PRIMARY KEY

A PRIMARY KEY means:

```text
UNIQUE
+
NOT NULL for every key column
```

The catalog records the semantic primary-key constraint as its own `ConstraintId`. The architecture also creates a unique B+ index implementing key lookup/enforcement. Its normalized catalog representation and exact UNIQUE/PRIMARY KEY/NOT NULL authorities are §16.5.4–§16.5.6.

The physical index and semantic primary-key constraint remain distinct catalog objects.

Every PRIMARY KEY component is runtime-enforced NOT NULL before duplicate checking. Fully non-NULL PRIMARY KEY values then use exactly §11.10's UNIQUE current-owner predicate, key equality, current-command behavior, UPDATE self-exclusion, wait/recheck protocol, and terminal lock lifetime.

## 21.8 CREATE INDEX

### 21.8.1 Bound-index handoff

The CREATE INDEX key is an ordered list of base-table columns. Expression
indexes are outside the architecture.

CREATE INDEX execution consumes the immutable index specification produced by
§§19.4.4 and 19.20:

```text
target TableId
ordered key ColumnIds
uniqueness
canonical index name
```

Chapter 21 does not repeat table/name lookup, key-column resolution, duplicate-
key rejection, or indexable-type validation.

The eventual catalog publication changes the target table's applicable index
manifest and therefore participates in §14.17.1's short object/statistics
publication gate. An ANALYZE that has already authorized row publication makes
CREATE INDEX wait through that ANALYZE transaction's terminal outcome; an index
manifest published first makes ANALYZE's final revalidation fail.

Persistent IndexId/FileId allocation occurs only during execution. A unique index uses Chapter 11's transactional uniqueness semantics. Its key equality and NULL rule are §11.10.2; UNIQUE constraints are immediate and non-deferrable.

The persistent index row and zero-based ordered key-column rows are exactly §16.5.4–§16.5.5; a standalone unique index is distinct from a constraint-owned backing index as specified there.

### 21.8.2 Offline build protocol

The architecture builds indexes offline with conservative writer exclusion:

```text
1. acquire SchemaLock
2. acquire target TableWriterGate exclusive
3. wait until pre-existing target-table writers are terminal
4. revalidate table identity and index-name availability against the canonical
   current catalog owner view and names reserved by this transaction's earlier
   DROP statements
5. allocate IndexId and B+ FileId
6. create one private empty B+ tree
7. enumerate the complete current logical owner set in the transaction-local
   target-table state at this command boundary
8. encode keys using the table/index descriptors
9. insert every physical (key,RID) through ordinary B+ MTRs
10. validate uniqueness when requested
11. flush/synchronize and durably rename the private B+ file to its exact
    final name through §4.7.4
12. acquire MANIFEST_CHANGE(TableId) through §11.13's unified graph; establish
    §14.17.1 VACUUM publication exclusion, drain any admitted mutating pass,
    then revalidate the complete current table/index manifest
13. only then install transaction-owned catalog index rows/constraint links
14. finish the DDL statement while retaining target-writer/DDL/publication ownership
15. before COMMIT, reverify that the required B+ file remains durably
    final-name-published
16. if/when the owning transaction reaches terminal COMMITTED publication:
        publish the index descriptor/name entry
17. on ABORT:
        keep catalog rows invisible and retire the private/final-uncommitted index file later
18. release target writer gate, MANIFEST_CHANGE, VACUUM admission exclusion,
    and SchemaLock only after terminal disposition according to §15.5 C5 /
    §15.6 A3; waiting passes resolve the resulting committed manifest
```

Step 7 uses a special DDL current-owner build view after target writers have
drained. It is not ordinary SQL snapshot visibility, committed-only visibility,
or raw physical-version enumeration. Its complete row set contains current
live owners committed by other transactions and the DDL transaction's own
live rows from earlier commands, including current replacement versions from
earlier UPDATE commands. It excludes aborted versions, superseded historical
versions, rows deleted by an earlier command of the DDL transaction, and
nonterminal versions owned by other transactions that cannot belong to this
transaction's committed table state.

The build view is fixed at the CREATE INDEX command boundary. An older
REPEATABLE READ query snapshot MUST NOT omit current owners required for index
publication; ordinary REPEATABLE READ SELECT visibility is unchanged. CREATE
INDEX performs no table-row mutation to include recursively, and the build view
does not invent visibility of a later command. For a table created earlier by
the same transaction, the retained exclusive writer gate from §21.6.2 is
reused, and intervening DML shared ownership was already subsumed rather than
upgraded.

For a UNIQUE build, step 10 applies §11.10.9 over exactly this same build set:
fully non-NULL duplicate groups use the canonical encoded equality/current-
owner rules, while any-NULL keys do not conflict. The exclusive writer gate
permits any conforming complete duplicate check; neither build-row order nor a
physical scan algorithm is semantic.

Readers may continue while the index is built because the private index is not visible to their catalog snapshots. New target-table writers are blocked until the index commit/abort boundary, preventing missing entries.

At committed catalog publication, every current logical owner belonging to the
DDL transaction's committed target-table state MUST have its required entry in
the index. In particular, CREATE TABLE, INSERT, and CREATE INDEX in successive
commands of one transaction cannot commit an index that omits the inserted
rows.

A half-built index is never visible to planning. If build/transaction aborts, the private or final-uncommitted index file becomes an orphan-retirement candidate.

## 21.9 DROP and physical object retirement

DROP TABLE / DROP INDEX is transactional at catalog visibility.

The `LIVE -> RETIRING` admission transition, active-maintenance drain, and
ANALYZE publication race are governed canonically by §14.17.1. This section owns
the catalog/physical retirement sequence after that coordination point.

Under DDL exclusivity:

```text
acquire SchemaLock
acquire the owning table's TableWriterGate exclusive
acquire MANIFEST_CHANGE for the table/index scope
revalidate current object identity, manifest, and LIVE state
for standalone DROP INDEX, reject an IndexId referenced by a live
    PRIMARY KEY or UNIQUE sys_constraints row
only then publish LIVE -> RETIRING for the affected identity
mark the relevant catalog rows deleted by the DDL transaction
do not unlink physical files immediately
commit/abort normally
```

`DROP TABLE` uses the table plus all current dependent IndexIds as one manifest
scope. `DROP INDEX` uses its owning TableId and that IndexId. All transaction
waits in the sequence use §11.13's graph; a DROP in the same transaction after
published ANALYZE statistics for the affected scope is rejected before waiting.
Once RETIRING publishes, no new object/statistics claim is admitted. Aborting
the DROP restores LIVE only through the object-publication protocol, and gate
release still waits for ABORTED publication.

A constraint-owned backing index is an index referenced by a live PRIMARY KEY
or UNIQUE constraint through §16.5.6. Although its canonical §16.5.4 index name
is NULL and therefore is not ordinarily name-resolvable as a standalone index,
standalone DROP INDEX MUST reject any resolved IndexId with that ownership.
The dependency check precedes catalog deletion, descriptor retirement,
`LIVE -> RETIRING`, and physical-file retirement eligibility. Rejection neither
drops nor mutates the constraint, deletes an index catalog row, nor changes the
file lifecycle. It reports the existing `CatalogError` family at the DROP INDEX
target-name construct. DROP TABLE remains one semantic operation that retires
its table, dependent constraints, and backing indexes.

On abort, the deletion `xmax` is ineffective and the object remains. On commit, new catalog snapshots no longer resolve the object, while older snapshots/descriptors may still legally reference it.

Once a DROP statement has successfully made its catalog deletion visible to
its owning transaction, that transaction retains a reservation for the
dropped canonical name through COMMITTED or ABORTED publication. CREATE TABLE
or CREATE INDEX in a later command of the same live transaction rejects the
reserved name even though the predecessor is no longer visible to that command.
On abort, the predecessor deletion is ineffective and no replacement identity
exists. After committed DROP, a later transaction may reuse the name subject to
ordinary current-owner revalidation; the replacement receives fresh
nonreusable object IDs and FileIds.

Name availability after committed DROP is independent of predecessor resource
retirement. Old snapshots and immutable descriptors may retain the predecessor,
and its file may remain `RETIRED_LINKED`, open, pinned, or awaiting unlink,
while a later object with the same SQL name has distinct persistent identity
and physical resources. Immediate unlink is not a condition of name reuse.

Physical file retirement therefore waits until:

```text
the dropping catalog row/version is globally dead to registered SQL snapshots
AND
no immutable descriptor/query handle can reference the physical object
AND
no BufferPool/page/file owner still requires it
```

The no-new-fetch/pin and frame-drain boundary is §7.12.5. Only after that drain may the managed file be unlinked through §4.7.7. Physical retirement is durable only after `fsync(database_root)` succeeds. If unlink or directory synchronization fails, semantic DROP remains committed and cleanup remains pending/retryable.

This retirement rule applies to table heap/FSM files and index B+ files. `DROP TABLE` also retires dependent indexes/constraints as one DDL semantic operation.

## 21.10 Catalog cache publication

The canonical catalog-cache visibility and descriptor-lifetime rules are §16.10.

DDL execution integrates with those rules as follows:

```text
uncommitted CREATE/ALTER-like metadata:
    transaction-local/catalog-MVCC-visible only

terminal COMMITTED:
    publish/replace current committed cache/name entries

terminal committed DROP:
    remove the object from current-name lookup
    while older immutable descriptors may remain alive
```

DDL MUST NOT mutate descriptors already retained by active plans.

After durable COMMIT, failure to allocate/install a new cache entry cannot change the transaction outcome. The coordinator must safely publish the new immutable entry or invalidate/bypass affected cache lookup so §16.10's snapshot-aware catalog path remains authoritative before COMMIT acknowledgement. Failure to establish either coherent state is database-noncontinuable under §39.1.5.

## 21.11 INSERT bound-statement handoff

For:

```sql
INSERT INTO t(a,c) VALUES (...);
```

INSERT execution consumes the fully bound statement produced by §§19.4.3 and
19.20. It receives the resolved target identity, canonical full target-column
order, bound source expressions, selected §17.8.5 assignment coercions, and
per-omitted-column persisted-default or typed-NULL action. Execution applies
that mapping to construct each candidate row; it neither resolves target names
nor selects a different conversion.

`INSERT ... SELECT` uses the same canonical target-column contract after the
source relation has been bound independently.

INSERT VALUES/SELECT expressions may contain only §20.14's supported
uncorrelated expression subqueries. Each occurrence retains lazy once-per-
attempt ownership; it is not reevaluated for every inserted row.

## 21.12 Default expressions

Column-default syntax may contain:

```text
literal constants
casts
registered immutable scalar operators
```

but it must be a **closed expression**.

A default MUST NOT contain:

```text
table-column references
subqueries
aggregates
scalar functions (the registry is empty)
parameters
```

Because every permitted default is closed and IMMUTABLE, DDL binding evaluates/constant-folds the complete expression exactly once under §17.10.2.

The result is then coerced by §17.8.5's assignment matrix to the target column
type before encoding.

If folding, casting, or constraint validation fails, the DDL statement fails rather than storing a partially resolved expression. Execution of INSERT therefore consumes one persisted typed constant default and does not reopen/re-resolve an operator/function tree.

This keeps the persistent default format small and stable while preserving the semantics of immutable default expressions.

### 21.12.1 DefaultValueBlob v1

The persisted default value is a byte sequence with this 24-byte header:

| Offset | Size | Field / meaning |
|---:|---:|---|
| `0` | 8 | magic = ASCII `DBLUSDEF` |
| `8` | 2 | format_version = `1` |
| `10` | 2 | flags = `0` |
| `12` | 4 | total_length |
| `16` | 4 | checksum_crc32c |
| `20` | 4 | reserved32 = `0` |
| `24` | variable | one §17.13 PersistedScalarV1 |

All multi-byte integers are little-endian. `total_length` is exactly: `24 + encoded PersistedScalarV1 length` with no bytes after the encoded scalar inside the blob.

CRC32C is computed over exactly `total_length` bytes with bytes `16..19` logically zero. The default representation imposes: `MAX_DEFAULT_VALUE_BLOB = 4096 bytes` so default metadata remains bounded and can fit the ordinary system-catalog storage path without introducing a separate large-object subsystem.

A larger default value is rejected as unsupported in the defined architecture.

On reopen, the decoder validates:

```text
magic/version/flags/reserved
length/checksum
PersistedScalarV1 structure
scalar TypeId == target column TypeId
```

before exposing the default to binding/execution.

Default format dispatch is exact under §4.14.2. With `DBLUSDEF` magic, version zero or malformed version-1 bytes are corrupt required default metadata; a positive version greater than one is `UNSUPPORTED_DEFAULT_FORMAT`. Either prevents reconstruction of the owning required column descriptor. V1 does not parse an expression-bearing blob as a scalar or ignore unknown trailing bytes.

Original SQL text MAY additionally be retained for display/debugging, but it is not execution authority.

An expression-bearing persistent default requires a distinct blob version with
stable function/operator identities. The architecture stores no such expression tree because
only the fully folded, final destination-typed result is persistent authority.

## 21.13 UPDATE operation planning

UPDATE planning consumes the fully bound target and SET/WHERE/RETURNING
expressions from §§19.4.3 and 19.20. Each assignment supplies a resolved
`ColumnId`, typed bound right-hand expression, and the already-selected
§17.8.5 assignment coercion. Chapter 21 does not rebind target or expression
names, reject duplicate SET targets again, or choose a new conversion.

UPDATE WHERE, assignment, and RETURNING expressions may use §20.14's supported
uncorrelated forms. WHERE evaluation remains in target materialization.
Demanded assignment and RETURNING expression/subquery failures participate in
the complete ordinary DML candidate closure required by §21.16.1 before the
statement's first persistent user-DML write.

Each assignment becomes:

```text
ColumnId -> typed bound expression
```

Unmentioned columns retain their old values. Logical planning requests target RID, old values needed by assignment expressions, old values needed to construct the complete new physical tuple, and values needed by RETURNING.

For every distinct finalized UPDATE target, the operation uses this semantic
sequence:

```text
revalidate/finalize the old target under the §15.3 target rules
evaluate every SET right-hand expression against that complete old-row image
apply the bound §17.8.5 assignment coercions
construct the complete candidate replacement row, copying every unmentioned
    column from the old row
validate every column whose bound target descriptor declares NOT NULL
only then admit the target to immediate UNIQUE preparation and ordinary
    candidate reduction
```

The NOT NULL check is descriptor-wide, not limited to PRIMARY KEY columns,
explicit SET targets, or indexed columns. It occurs after simultaneous
assignment evaluation and complete-row construction, so `SET a=b, b=a`
continues to read both values from the old row. An unmentioned nonnullable
column retains its old value and is checked with the rest of the candidate.

A NULL in any descriptor-nonnullable candidate column reports the canonical
§39.3 `ConstraintViolation` family used by INSERT NOT NULL enforcement. No new
tuple version, old-version `xmax/cmax`, or new referring index entry for that
target may publish. For an explicitly assigned column, the responsible
diagnostic origin is the SET right-hand/value expression whose final coerced
value is NULL, retaining its Chapter-19/20 SourceSpan and provenance. In an
architecture-valid old row, a copied unmentioned nonnullable column cannot be
NULL; structural detection of the contrary remains an existing descriptor/
tuple corruption condition and does not invent a synthetic SQL offset.

All finalized targets complete this ordinary candidate-producing work before
any target mutation publishes. For example, if target A has a valid candidate
but target B produces a SET or NOT NULL candidate, Chapter 21 reduces that
closed domain and no mutation for A publishes. This rule adds no physical
statement undo. A value-preserving assignment such as `SET x=x` still acts on
and counts the finalized target under §15.3 after its complete row passes the
same validation.

The resulting `LogicalUpdate` feeds §15.3's physical UPDATE version protocol.

## 21.14 DELETE operation planning

DELETE planning consumes the fully bound target, optional BOOLEAN WHERE
predicate, and RETURNING metadata from §§19.4.3 and 19.20. Chapter 21 performs
no target, predicate, or RETURNING name/type resolution.

DELETE WHERE and RETURNING may use only §20.14's supported uncorrelated forms. The logical child produces target RID plus old values needed by RETURNING or semantic evaluation. DELETE does not physically erase index entries.

Vacuum remains the index-garbage owner. DELETE targets one base table.

## 21.15 RETURNING

The logical DML structures support:

```sql
INSERT ... RETURNING ...
UPDATE ... RETURNING ...
DELETE ... RETURNING ...
```

RETURNING expressions bind against the DML-defined row image:

```text
INSERT -> inserted/new row
UPDATE -> updated/new row
DELETE -> deleted/old row
```

Every DML RETURNING result is a bag of row occurrences and establishes no
semantic row order:

```text
INSERT -> one final-new-row occurrence per successfully inserted logical input
          occurrence in the final successful statement attempt
UPDATE -> one final-new-row occurrence per distinct finalized target acted upon
DELETE -> one retained-old-row occurrence per distinct finalized target
```

An ordered INSERT source does not transfer its order to RETURNING. UPDATE
target discovery duplicates do not create duplicate result occurrences, and
RID order, source encounter order, spool order, mutation order, and physical
reclamation order are nonsemantic. Executions that produce different sequences
with equal RETURNING bags are equivalent. This freedom changes neither row
images, multiplicity, affected-row count, errors, nor the statement-result
envelope.

An uncorrelated subquery inside RETURNING cannot capture that row image; only
ordinary local RETURNING column references can. The side plan remains an
independent query block under §20.14.2.

The exact physical buffering mechanism is defined by the execution stage.

The §15.7 statement-attempt retry rule still applies: before persistent statement writes READ COMMITTED may restart internally; after persistent writes the transaction aborts rather than replaying the same attempt under the same TxnId.

No externally visible RETURNING row is emitted from an attempt that may still restart.

Every semantically demanded RETURNING expression is evaluated, and its exact
row value retained in statement-owned storage, as part of §21.16.1 ordinary
candidate closure before the first persistent user-DML write. Thus a demanded
RETURNING expression error can neither follow an earlier mutation from that
statement nor expose a partial result. The physical layer continues to buffer
the retained rows through successful statement completion as specified in
§31.9.

Dynamic memory/spill failure while constructing or finalizing the unpublished
spool follows §39.1's actual statement-write boundary; it is not an ordinary
RETURNING candidate. A returned row is never a COMMIT acknowledgement.

## 21.16 Error contract for semantic planning

Front-end semantic failures are classified rather than collapsed.

Important categories include:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature
CardinalityError
```

Where source syntax exists, the error carries the smallest useful source span and a human-readable message. Storage/transaction errors retain their lower-layer category and may be mapped to SQLSTATE-like surface codes.

### 21.16.1 Ordinary multi-row DML runtime-error precedence

An **ordinary row-semantic error candidate** is a correctness error from a DML
row occurrence whose source/target occurrence belongs to the finalized
statement attempt and whose binding, logical, target-revalidation, and other
semantic prerequisites have succeeded far enough to establish that error.
This domain includes demanded scalar arithmetic, cast, and conversion errors;
INSERT input and `INSERT ... SELECT` source-expression or supported-subquery
errors; UPDATE assignment and coercion errors; demanded target-qualification
ordinary errors not already closed while producing the target domain;
demanded RETURNING expression errors; descriptor-wide final-row NOT NULL
violations; immediate UNIQUE/PRIMARY KEY violations; and any other supported
deterministic row constraint with an existing owner.

Errors from abandoned retry attempts, target occurrences removed by canonical
deduplication, stale targets not retained as finalized targets, and skipped or
nonqualifying occurrences are not candidates. Cancellation,
`FrontEndResourceLimit`, `OutOfMemory`, deadlock victimization, storage/I/O
failure, transaction-state failure, stale-target or serialization failure, and
other independently owned dynamic failures retain their Chapter-11/15/39
behavior and are not reordered by this ordinary-candidate rule.

Every ordinary candidate carries the responsible source diagnostic selected by
its scalar, subquery, or constraint owner. Among candidates for one final
statement attempt, the public error is selected by:

1. smallest responsible `SourceSpan.start_byte_offset`;
2. for equal starts, the shorter and therefore more-specific represented
   `SourceSpan`;
3. for an identical responsible span, this row-semantic phase order:

```text
1. scalar/subquery/final-row expression evaluation
2. descriptor-wide final-row NOT NULL validation
3. immediate UNIQUE validation
4. any other already-supported row constraint class
```

An existing more-specific owner keeps its internal precedence within a phase.
If start, specificity, phase, public category, and diagnostic origin are all
identical, the candidates are observationally equivalent; Chapter 21 adds no
RID, page, source-row index, spool/vector/batch position, hash/index order,
physical-operator order, or scheduler tie-breaker.

Semantic ordering of a Chapter-20 DML input does not replace this diagnostic
precedence with first-row-encountered behavior. An unordered input remains an
unordered bag. Within one executable scalar occurrence, Chapter 17 and Chapter
20 continue to own child evaluation and short-circuit order; this section only
arbitrates already-defined ordinary candidates across DML occurrences.

Physical visitation MUST NOT let work associated with a lower-precedence
ordinary candidate cross the first-persistent-user-DML-write boundary. Before
that boundary, the final attempt MUST establish every semantically demanded
ordinary candidate in its complete input/finalized-target domain and reduce
the closed set by the ordering above. If any candidate wins, the statement
fails before that boundary. Only a closure with no winning ordinary candidate
authorizes persistent user-DML publication. This is an observable semantic
requirement; §§15.1.3 and 31.4–31.9 own its memory-bounded physical staging
without prescribing one container, vector width, or spill organization.

Consequently, physical RID/input visitation, batching, worker scheduling,
target order, source encounter order for an unordered relation, spill order,
hash iteration, lock acquisition detail, and mutation order cannot change
either the selected ordinary error or whether it receives §39.1's pre-write
versus post-write consequence. Chapter 20 demand remains authoritative: closure
does not evaluate an occurrence or side plan proven semantically undemanded.

Normative examples are:

```text
INSERT VALUES: first candidate valid, later candidate divides by zero
    -> division candidate closes before publication; no inserted row publishes

INSERT SELECT: source yields valid candidates, then a demanded conversion error
    -> no target row publishes before the complete demanded source closes

UPDATE: target A valid, target B produces SET/NOT NULL/RETURNING error
    -> the winner is reduced before either target mutation publishes
```

Immediate UNIQUE remains lock-protected current-state validation, not a
deferred final-state constraint. Chapter 11's exact pending-owner relation
establishes same-statement staged collisions before publication, while current
old owners and earlier-command owners remain authoritative.

The rule does not require prediction or source-span ranking of a later
deadlock, I/O failure, concurrency invalidation, resource exhaustion,
cancellation, corruption, or other dynamic condition whose occurrence is not
yet established. Such failures remain owned at their actual runtime stage and
may occur before or after publication. One CommandId per admitted statement,
CommandId reuse across internal retry, discarded abandoned-attempt errors and
results, the pre-write retry boundary, and the post-write mandatory-abort rule
remain unchanged.

## 21.17 Front-end handoff

Chapter 18 owns lexing, parsing, request/batch framing and recovery, raw-AST
construction, and parse failures; the relevant boundaries are §§18.10.1,
18.13, and 18.17.2–18.17.3. Chapter 21 applies only after Chapter 18 has
produced an independent raw statement and Chapter 19 has successfully bound
it. Chapter 21 defines no token or semicolon synchronization algorithm.

A parse failure therefore produces no Chapter-21 operation and is not a DML
runtime failure. Statement admission, CommandId allocation, and transaction
consequences remain owned by §§9.6 and 39.1.

### 21.17.1 ANALYZE bound-statement and transaction boundary

For:

```sql
ANALYZE table_name;
```

Chapter 21 consumes the fully bound statement produced under §19.1 for exactly
one base table visible through the normal catalog snapshot.

The bound statement captures:

```text
TableId
immutable TableDescriptor
current SchemaVer
analyzed ColumnIds
currently visible IndexIds
```

These stable identities are scan inputs, not permanent publication authority.
The final current-object/schema/index-manifest revalidation and transaction-
lifetime statistics publication claim are mandatory under §14.17.1.

ANALYZE is not schema-changing DDL and does not acquire `SchemaLock` or an exclusive `TableWriterGate` merely to obtain a stable row set.

Its SQL-visible values come from the transaction's normal effective snapshot:

```text
READ COMMITTED:
    statement snapshot

REPEATABLE READ:
    transaction snapshot + current command boundary
```

Concurrent DML may make the completed statistics immediately approximate/stale; it does not invalidate query correctness. ANALYZE writes its `sys_statistics` rows through the ordinary transaction/catalog MVCC path. The newly built `StatsDescriptor` remains transaction-local until the owning transaction commits.

For an explicit transaction:

```text
successful ANALYZE statement
    -> own later statements may use the transaction-local descriptor
    -> global committed statistics cache remains unchanged
    -> COMMIT publishes the descriptor globally
    -> ABORT discards it
```

After statistics rows publish, the transaction retains their `STATS_PUBLISH`
claims through terminal publication. A later `CREATE TABLE` or other DDL on an
unrelated manifest remains legal and any SchemaLock wait uses §11.13's unified
graph. A later DDL that would change/drop the analyzed table or one of its
manifest indexes is rejected before waiting because it would invalidate the
same transaction's immutable generation. DDL completed first may be followed by
ANALYZE only when final revalidation observes the exact transaction-local
post-DDL manifest.

For autocommit, global publication occurs only after the statement's owning transaction reaches terminal COMMITTED.

A failed/cancelled ANALYZE publishes neither a partial descriptor nor a global cache entry. If it had already published any transaction-owned `sys_statistics` row, §39.1 requires automatic transaction abort; otherwise the explicit transaction may remain `ACTIVE`.

## 21.18 SQL statement surface

The statement surface includes:

```text
CREATE TABLE
CREATE INDEX / CREATE UNIQUE INDEX
DROP TABLE / DROP INDEX
INSERT
UPDATE
DELETE
SELECT
WHERE
INNER JOIN
LEFT JOIN
CROSS JOIN
GROUP BY
HAVING
ORDER BY
DISTINCT
LIMIT / OFFSET
BEGIN
COMMIT
ROLLBACK
VACUUM
ANALYZE
EXPLAIN
EXPLAIN ANALYZE
```

The architecture executes the exact uncorrelated scalar, EXISTS/NOT EXISTS, single-column
IN/NOT IN, and derived-table forms in §20.14. That matrix—not generic parser or
IR capacity—is the complete subquery surface.

The purpose of this surface is to exercise the real relational engine, not to claim broad SQL-standard compatibility.

## 21.19 SQL/front-end features outside the defined architecture

The architecture excludes:

```text
ALTER TABLE
foreign keys
CHECK constraints
views / materialized views
triggers
stored procedures
recursive CTEs
window functions
RIGHT/FULL OUTER JOIN
NATURAL/USING JOIN
LATERAL
MERGE
UPSERT / ON CONFLICT
generated columns
sequences / identity columns
expression indexes
partial indexes
collation framework
time zones
DECIMAL / NUMERIC
INTERVAL
JSON
arrays
user-defined types
prepared-statement parameters
SQL privileges / roles
correlated-subquery execution/decorrelation
row-valued or multi-column subqueries
ANY / SOME / ALL quantified comparisons
derived-column alias lists
CTEs and SQL set operations
data-modifying subqueries
```

These features are not implicit requirements of the SQL/front-end contract.

## 21.20 Upper semantic-layer invariants

1. Parser output contains syntax names; binder output contains resolved IDs/types.
2. Executor does not perform SQL name resolution.
3. Logical operators never encode physical algorithms.
4. ColumnId, BindingId, LogicalSlotId, heap SlotId, and physical vector position are distinct.
5. Self-joins use distinct BindingIds.
6. All bound expressions have resolved type/nullability before logical planning.
7. Non-BOOLEAN WHERE/HAVING/ON predicates are rejected.
8. Numeric implicit casts occur only in §17.8.5's closed contexts, widen, and do not silently narrow.
9. `SELECT *` is expanded before logical planning.
10. Aggregate/grouping legality is validated before execution.
11. LEFT JOIN right-side outputs are nullable.
12. Hidden DML target RID slots survive until the DML operator consumes them.
13. Catalog visibility follows the caller's MVCC snapshot; cache hits cannot bypass it.
14. Uncommitted DDL is never globally published as committed metadata.
15. Schema-changing DDL is conservatively serialized in the defined architecture.
16. Offline CREATE INDEX blocks target-table writers and covers the complete transaction-local current-owner row set that can become committed with the DDL transaction.
17. CREATE abort may leave physical garbage but never a visible half-created object.
18. DROP file unlink waits until old catalog snapshots/descriptors can no longer reference the object and is not durably complete until its parent directory is synchronized.
19. Catalog-object IDs/FileIds that may have entered persistent state are never reused.
20. Defaults persist the fully folded typed scalar result; runtime function/operator identities are not persisted as default authority.
21. ANALYZE uses normal catalog/MVCC visibility and publishes a committed statistics descriptor only at transaction terminal COMMITTED.
22. Rewrites preserve NULL, volatility, outer-join, grouping, and hidden-slot semantics.
23. Logical-plan validation detects broken slot/schema references before execution.
24. Unsupported SQL fails explicitly rather than being partially reinterpreted.
25. CREATE catalog commitment is forbidden until every required physical file has completed durable final-name publication under §4.7.
26. DML accepts only §20.14's uncorrelated expression subqueries; defaults/DDL expressions reject every subquery, and §39.1 alone owns any runtime error consequence.
27. Every finalized UPDATE candidate satisfies every descriptor NOT NULL constraint before its replacement, old-version delete metadata, or referring index entries publish.
28. A live PRIMARY KEY/UNIQUE constraint's backing index cannot be removed by standalone DROP INDEX.
29. Every semantically demanded ordinary DML candidate in the final attempt is
    established and canonically reduced before the first persistent user-DML
    write; physical visitation cannot select the winner or change its §39.1
    consequence.
30. DML RETURNING is an unordered bag with operation-specific row-image and occurrence multiplicity semantics.
31. A canonical name dropped by a live transaction remains reserved against recreation by that transaction through terminal outcome.
32. Committed DROP name reuse, predecessor descriptor lifetime, and predecessor physical-file retirement are independent; replacement objects always have fresh persistent identities.
