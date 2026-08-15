• # Deep Architecture Semantic Review

  ## A. Executive verdict

  ARCHITECTURE SEMANTIC REVISION REQUIRED

  docs/ARCHITECTURE.md is unusually detailed and largely coherent. Its persisted heap, tuple, FSM, B+ key, WAL record, control-file, statistics-payload, SQL
  NULL, FLOAT64, execution-property, and optimizer-capability contracts are generally strong.

  It is not yet sufficient to implement the complete intended v1 without inventing core semantics. Three blocking defects remain:

  1. transaction-status page mutation contradicts the universal full-page-image invariant;
  2. stale statistics may incorrectly prove a relation empty;
  3. durable file/WAL-segment namespace publication is not defined strongly enough for acknowledged durability.

  There are also major underspecifications in the exact BufferPool state machine, catalog system-relation schemas, WAL/MTR failure handling, uniqueness
  checking, statement-error effects, type/cast semantics, and several lifecycle/concurrency boundaries.

  Overall:

  - Semantic completeness: high, but incomplete at several critical boundaries.
  - Internal consistency: generally strong; three substantive contradictions/holes found.
  - Technical correctness: sound in most subsystems, but the blocking defects can cause wrong query results or unrecoverable acknowledged state.
  - Implementability: Phase 1 formats are implementable; complete v1 is not independently implementable yet.
  - Clarity: strong locally, weaker where subsystem protocols meet.
  - V1 closure: incomplete.
  - BufferPool specifically: blocked by the missing resident-frame/I/O failure state machine, independently of later transaction/catalog findings.

  No files were modified. Implementation Phase 2 was not entered.

  ## B. Blocking architecture defects

  ### A-001 — Transaction-status mutation violates the full-page-image invariant

  - Severity: BLOCKING ARCHITECTURE DEFECT
  - Architecture sections: docs/ARCHITECTURE.md:5015, docs/ARCHITECTURE.md:6260, docs/ARCHITECTURE.md:6515, docs/ARCHITECTURE.md:7053, docs/ARCHITECTURE.md:7932
  - Exact problem: §12.10 requires every clean-to-dirty ordinary page mutation to begin with PAGE_IMAGE or PAGE_INIT, and requires rec_lsn to identify that
    complete image. COMMIT/ABORT instead modify a TXN_STATUS page using the zero-payload TXN_COMMIT/TXN_ABORT record LSN, with no complete page image.

  - Failure scenario:
      1. A clean status page is modified by COMMIT.
      2. Its page_lsn becomes the commit LSN, but that record contains no page image.
      3. The status page later suffers a torn write.
      4. Older terminal WAL records needed to reconstruct the rest of the page have been recycled.
      5. Recovery requires a retained full image under §13.14, but no such image began this dirty interval.

  - Why insufficient: the terminal record can repair one transaction’s bits, but it cannot reconstruct every other status entry on a corrupt 8 KiB status page.
  - Exact decision required: either:
      - define a canonical status-page WAL/FPI protocol that produces a complete image whenever the status page enters a dirty interval, including exact
        page_lsn and rec_lsn assignment; or

      - explicitly exempt status pages from the FPI invariant and define a complete independent reconstruction and WAL-retention protocol. The current
        architecture does neither.

  ### A-002 — Stale statistics can authorize a wrong-result empty plan

  - Severity: BLOCKING ARCHITECTURE DEFECT
  - Architecture sections: docs/ARCHITECTURE.md:14385, docs/ARCHITECTURE.md:15037, docs/ARCHITECTURE.md:15200
  - Exact problem: §34 declares that statistics may be stale and must never change query results. §35.6.3 nevertheless allows statistics min/max to set
    is_provably_empty = true.

  - Failure scenario:
      1. ANALYZE records max(x)=100.
      2. A committed INSERT adds x=200.
      3. The stale statistics remain valid planning metadata.
      4. WHERE x=200 is marked provably empty.
      5. The optimizer replaces the scan with an empty plan and returns the wrong result.

  - Why insufficient: is_provably_empty is a semantic property, not a cost estimate.
  - Exact decision required: approximate or stale statistics may estimate zero but MUST NOT prove emptiness. Only logical contradictions, literal truth, LIMIT
    0, and currently enforced semantic constraints may set is_provably_empty. The same correction must cover range and join-domain disjointness inferred from
    statistics.

  ### A-003 — Durable file and WAL-segment namespace publication is incomplete

  - Severity: BLOCKING ARCHITECTURE DEFECT
  - Architecture sections: docs/ARCHITECTURE.md:2640, docs/ARCHITECTURE.md:5888, docs/ARCHITECTURE.md:10685
  - Exact problem: v1 uses newly created object files and WAL segment files, but only specifies fdatasync on files. It does not specify durable parent-directory
    updates, atomic naming/publication, or recovery-driven recreation of a committed missing file.

  - Failure scenario:
      1. CREATE TABLE initializes private heap/FSM files.
      2. Catalog changes and COMMIT WAL become durable.
      3. COMMIT returns.
      4. A crash loses an unsynchronized directory entry for one newly created file.
      5. Recovery sees committed catalog metadata referencing a missing object file.

  - The same problem applies when a new WAL segment is created: fdatasync of the segment does not by itself define durability of its directory entry.
  - Why insufficient: the acknowledged-commit guarantee in §13.20 cannot be met if the durable WAL or committed object filename can disappear.
  - Exact decision required:
      - define parent-directory synchronization and naming rules for creation, rename/publication, unlink, and WAL-segment creation; or
      - define WAL-backed file-creation metadata sufficient for recovery to recreate missing committed files.
      - A private physical object must be durably recoverable before catalog commit can be acknowledged.

  ## C. Major underspecifications

  ### M-001 — BufferPool resident-frame and I/O state machine

  - Section: docs/ARCHITECTURE.md:2593
  - Ambiguous choice: I/O state is listed as frame metadata but its states and transitions are never defined.
  - Plausible incompatible implementations:
      - duplicate concurrent reads install two frames for one PageId;
      - one loader publishes a placeholder and all other fetches wait;
      - a failed loader leaves a poisoned mapping;
      - a failed load removes the mapping and permits retry.

  - Missing rules include:
      - one-loader/many-waiter fetch behavior;
      - when PageId -> FrameId becomes visible;
      - victim reservation and removal from CLOCK/page table;
      - whether a flushing frame may be selected or reassigned;
      - load, flush, eviction, and WAL-flush failure transitions;
      - pin acquisition relative to frame publication;
      - when page-format/checksum validation occurs;
      - whether failed I/O leaves the frame retryable, resident-invalid, or fatal;
      - how dirty state survives failed writeback.

  - Required answer: lock a conceptual frame state machine and linearization points. This blocks BufferPool implementation specifically.

  ### M-002 — Catalog system-relation schema v1 is not byte/semantically complete

  - Section: docs/ARCHITECTURE.md:8225, docs/ARCHITECTURE.md:8605
  - Ambiguous choice: the architecture lists semantic field names but not the exact built-in schema descriptors needed to encode the six bootstrap relations.
  - Two implementations could choose different:
      - physical column order;
      - TypeIds;
      - nullability;
      - VARCHAR versus fixed encoding;
      - flag meanings;
      - default-blob storage form;
      - constraint-definition payload grammar.

  - This would create incompatible v1 databases despite both claiming catalog_schema_version=1.
  - Required answer: define the exact v1 SchemaDescriptor for every system relation, including stable ColumnIds, physical order, types, nullability, defaults,
    and exact payload formats.

  ### M-003 — MTR and page-append failure atomicity

  - Sections: docs/ARCHITECTURE.md:780, docs/ARCHITECTURE.md:6358
  - Ambiguous choice:
      - ordinary append may extend a file before PAGE_INIT append fails;
      - B+ MTR installs final runtime tree bytes before WAL append.

  - The no-flush barrier prevents crash leakage, but runtime error handling is unspecified.
  - Plausible implementations:
      - restore all before-images;
      - retain the barrier and fail the database;
      - truncate/retry the unpublished append;
      - mistakenly release the barrier and continue.

  - Required answer: define whether failure restores pre-mutation runtime state, retries while retaining exclusive ownership, or places the database in a
    noncontinuable/fatal state. Define cleanup of a physically extended but unpublished tail.

  ### M-004 — Unique-check current-state predicate is not exact

  - Section: docs/ARCHITECTURE.md:5740
  - Ambiguous choice: “another logically live row owns the key” is not a complete truth table.
  - Missing cases:
      - a key inserted earlier in the same statement;
      - a key inserted by an earlier command in the same transaction;
      - an UPDATE retaining the same unique key and encountering its own old RID;
      - old/new versions both owned by the same transaction;
      - self-deleted versions;
      - current-command versions that ordinary snapshot visibility deliberately hides;
      - an exact physical retry of the same (key,RID).

  - Incompatible outcomes: one implementation may permit duplicate keys inside one transaction because cmin == command_id is not ordinarily visible; another may
    reject a same-key UPDATE against its own old RID.

  - Required answer: define the unique-conflict predicate independently from ordinary snapshot visibility, including the operation’s excluded target RID/version
    where applicable.

  ### M-005 — Statement errors and transaction state are not fully specified

  - Sections: docs/ARCHITECTURE.md:7980, docs/ARCHITECTURE.md:17513
  - Ambiguous choice: the retry rule covers write conflicts, but general execution errors do not say whether they:
      - fail only the statement;
      - mark the transaction aborted;
      - permit later commands;
      - require abort when persistent statement writes already exist.

  - Example: the fifth row of a multirow INSERT raises an arithmetic error after four rows have been physically installed. Without statement undo, continuing
    and later committing the same TxnId would expose the first four rows.

  - Also ambiguous: after COMMIT WAL is durable, a subsequent status-page or cache-publication failure cannot legally turn the transaction into ABORTED, but the
    client-visible outcome is unspecified.

  - Required answer: define an error-to-statement/transaction-state matrix and an “outcome uncertain/fatal completion” rule for failures after durable commit.

  ### M-006 — V1 scalar operator, literal, and cast semantics are incomplete

  - Sections: docs/ARCHITECTURE.md:8819, docs/ARCHITECTURE.md:9245, docs/ARCHITECTURE.md:17569
  - Ambiguous choices include:
      - whether an integer literal that fits INT32 binds as INT32 or INT64;
      - result types of / and %;
      - signed integer quotient/remainder rounding rules;
      - the exact set of explicit casts supported in v1;
      - parsing/formatting rules for numeric, DATE, TIMESTAMP, and BOOLEAN casts;
      - scalar function names and signatures actually supported;
      - floating rounding-mode requirements for constant folding versus runtime execution.

  - “Initial explicit conversions may include” is too weak for a semantic contract.
  - This also affects persisted defaults because folded cast results become authoritative bytes.
  - Required answer: provide a closed v1 operator/cast/literal registry with result types and failure behavior.

  ### M-007 — Subquery support is neither clearly required nor clearly deferred

  - Sections: docs/ARCHITECTURE.md:9319, docs/ARCHITECTURE.md:10194, docs/ARCHITECTURE.md:11126
  - Ambiguous choice: scalar, EXISTS, IN, and derived-table subqueries are described as “initial semantic targets,” while the v1 surface merely remains
    “architecturally capable” of them.

  - There is no complete physical execution contract for scalar/EXISTS/IN subplans.
  - Required answer: either include these forms in v1 and define physical execution, NULL semantics, ownership, cardinality-error timing, and memory behavior,
    or classify execution as deferred while retaining only parser/IR compatibility.

  ### M-008 — Persisted structural validation has important gaps

  - Sections: docs/ARCHITECTURE.md:2475, docs/ARCHITECTURE.md:4445
  - Gaps:
      - overlapping NORMAL heap tuple ranges are not universally invalidated;
      - B+ local sorted order is only a debug/verifier SHOULD;
      - B+ entry-range overlap is not explicitly rejected;
      - the B+ free list lacks an explicit in-range, acyclic, unique-membership validation requirement;
      - a recognized REDIRECT_RESERVED heap slot has no canonical field semantics.

  - Two implementations may accept different logical corruption and may return wrong rows without invoking the offline full-tree verifier.
  - Required answer: distinguish:
      - invariants required before a page may participate in normal query execution;
      - expensive whole-tree checks;
      - reserved states that v1 must reject rather than merely recognize.

  ### M-009 — Transaction-status reclamation versus persisted StatsVersion

  - Sections: docs/ARCHITECTURE.md:7631, docs/ARCHITECTURE.md:14426
  - Ambiguous choice: statistics permanently persist (stats_txn_id, stats_command_id), while status reclamation requires proving no persistent correctness
    object references a normal TxnId below the cutoff.

  - Possible interpretations:
      - stats_txn_id requires status retention forever;
      - it is opaque identity only and does not count as a status-dependent reference;
      - old statistics must be rewritten or deleted before advancing the cutoff.

  - Required answer: explicitly classify StatsVersion TxnIds and state whether status lookup is ever performed for them after catalog-row visibility is
    established.

  ### M-010 — Identifier exhaustion is incomplete outside selected allocators

  - Sections: docs/ARCHITECTURE.md:354, docs/ARCHITECTURE.md:4747, docs/ARCHITECTURE.md:5888
  - TxnId, control generation, FileId, catalog IDs, and read epochs have overflow policies.
  - Missing:
      - CommandId exhaustion;
      - WAL LSN/end-position exhaustion;
      - final ordinary PageNo allocation before INVALID_PAGE_NO;
      - persistent uint32 total_length overflow policy in all builders.

  - CommandId wrap can directly break same-transaction visibility.
  - Required answer: fail or abort before each identity wraps or enters its sentinel domain.

  ### M-011 — Maintenance-operation coordination is incomplete

  - Sections: docs/ARCHITECTURE.md:7758, docs/ARCHITECTURE.md:11063, docs/ARCHITECTURE.md:13843
  - Missing interactions:
      - VACUUM versus VACUUM on the same table;
      - status-cutoff advancement by concurrent vacuum workers;
      - ANALYZE publication when DROP commits while ANALYZE holds an old descriptor;
      - publication of statistics for an object no longer current/visible.

  - Required answer: define relation/database maintenance ownership and the commit-time revalidation or discard rule for statistics cache publication.

  ### M-012 — V1 strictness is inconsistent for future/reserved states

  - Sections: docs/ARCHITECTURE.md:694, docs/ARCHITECTURE.md:1203
  - The superblock decoder preserves unknown flags although no v1 flag semantics are assigned. Most other v1 formats reject unknown bits.
  - REDIRECT_RESERVED is a recognized state but has no canonical coordinates or aux semantics and MUST NOT be emitted.
  - Incompatible implementations may reject these values, ignore them, or expose them as valid state.
  - Required answer: either make them strictly invalid for v1 readers or explicitly define them as safely ignorable and prove that old readers cannot
    misinterpret future semantics.

  ### M-013 — Statistics validation tolerance is not canonical

  - Section: docs/ARCHITECTURE.md:14877
  - “Except for explicitly tolerated small floating rounding” does not specify an epsilon or normalization rule.
  - MCV values are not explicitly required to be unique.
  - Two decoders may accept different persisted payloads.
  - Required answer: lock the exact aggregate-mass tolerance and require canonical unique MCV identities or define duplicate combination behavior.

  ### M-014 — Database open/exclusivity/shutdown state is incomplete

  - Sections: docs/ARCHITECTURE.md:276, docs/ARCHITECTURE.md:6957, docs/ARCHITECTURE.md:8637
  - “Single database process” is stated, but it is unclear whether that is:
      - an externally guaranteed precondition; or
      - an invariant enforced by an exclusive database lock.

  - Normal shutdown, clean-shutdown indication, open failure cleanup, and multiple-handle/process exclusion are not fully specified.
  - Required answer: lock the database lifecycle state machine and whether process exclusivity is detected or merely assumed.

  ## D. Minor ambiguities / clarity findings

  ### D-001 — “status page/cache” conflates two caches

  At docs/ARCHITECTURE.md:7932, “transaction-status page/cache” appears before the §9.14 terminal-publication step. If “cache” means the runtime terminal-
  outcome cache, this contradicts §9.14’s atomic publication point. Clarify that any pre-publication cache is only BufferPool/page state, not the globally
  observable terminal-outcome cache.

  ### D-002 — prune_hint reader behavior

  docs/ARCHITECTURE.md:1150 says it initializes to zero and has no correctness semantics, but does not say whether nonzero values are reader-valid and ignored.
  State this explicitly.

  ### D-003 — creation_epoch is opaque without a writer rule

  docs/ARCHITECTURE.md:660 reserves a persisted value but gives no generation rule. This is compatible while it remains semantically inert, but the document
  should say whether v1 writers use zero, a monotonic database value, or any arbitrary opaque value.

  ### D-004 — Rewrite-provenance language remains in the live architecture

  - docs/ARCHITECTURE.md:10656: “R-037 is resolved”
  - docs/ARCHITECTURE.md:12084: “Pass-13 operator contracts”
  - docs/ARCHITECTURE.md:18647: formats “are added as chapters are migrated”

  These references do not define semantics and should be rewritten as direct live-document statements.

  ### D-005 — Concept glossary

  Most identity domains are used consistently:

   Term                          Canonical meaning                                               Status
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Database                      One single-node database rooted by control/catalog/WAL state    Clear, lifecycle incomplete
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Relation/table                Logical catalog object backed by heap/FSM and indexes           Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   FileId                        Persistent database file identity                               Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   PageNo                        Page identity within one file                                   Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   PageId                        (FileId, PageNo)                                                Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Heap SlotId                   Persisted slot within a heap page                               Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   RID                           Physical heap tuple-version identity                            Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   TxnId / CommandId             Transaction and command identity                                Clear; CommandId exhaustion missing
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   BindingId                     Query-local relation occurrence                                 Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   LogicalSlotId                 Query-local semantic output value                               Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   RowCollection handle          Query-temporary row identity                                    Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   RelationSet bit               Optimizer identity for one BindingId                            Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Tuple                         Persisted tuple bytes/version                                   Mostly clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Logical row                   SQL row identity inferred through version history               Not given a first-class persistent identity
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Snapshot                      Txn horizon, active set, owner, command boundary                Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Read epoch                    Process-local physical-RID reuse protection                     Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Page/frame/pin/guard          Persistent bytes versus resident ownership                      Terms clear; state machine incomplete
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   User key / physical B+ key    SQL key versus (user key,RID)                                   Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   Persisted scalar              Catalog/statistics scalar codec                                 Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   StatsVersion                  (TxnId,CommandId) statistics identity                           Clear; reclamation interaction missing
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   OrderingProperty              Slot-based physical order                                       Clear
  ────────────────────────────  ──────────────────────────────────────────────────────────────  ─────────────────────────────────────────────
   RequiredSlotSet               Values required by an ancestor                                  Clear

  ## E. Non-blocking design risks

  1. Full image on every clean-to-dirty interval is correct but may generate substantial WAL after aggressive background flushing.
  2. One query-wide read epoch is safe but may delay RID reuse for long-running scans that no longer retain earlier RIDs.
  3. Holding the global SchemaLock through an explicit DDL transaction is simple and safe but permits severe DDL blocking.
  4. No physical user-DML undo simplifies recovery but creates potentially large aborted heap/index garbage and makes statement-error policy especially strict.
  5. Exact physical B+ MTRs are operationally clear but may produce large WAL records for broad structural operations.
  6. Query-memory spill contracts are semantically sound, but disk-full behavior needs to consistently route through the statement/transaction error matrix.
  7. Status-file absolute addressing plus sparse reclamation is coherent but depends on reliable filesystem hole-punch behavior only for space efficiency, not
     correctness.

  ## F. Global invariant matrix

   Invariant                                    Canonical owner    Dependent chapters            Status                          Notes
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Page size = 8192                             §4.2               5–8, 9, 12–16                 Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Page 0 is superblock                         §4.7               heap/FSM/B+/catalog/status    Consistent                      Ordinary-page exceptions clear
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   PageId identity                              §4.4               BufferPool, WAL, B+           Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   RID is physical version identity             §4.5               B+, MVCC, DML, vacuum         Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Reserved RID bytes zero/reject               §8.4.1             index codec                   Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Slot reuse waits for grace                   §14.5–14.12        heap, B+, scans               Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   WAL-before-data                              §12.17             BufferPool, all pages         Consistent except status FPI    A-001
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Every dirty interval starts with FPI         §12.10             BufferPool, recovery          Contradicted                    Status pages
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Page checksum covers stable image            §4.12              BufferPool/recovery           Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Transaction terminal publication             §9.14              locks, snapshots, DDL         Mostly consistent               “status cache” ambiguity
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Snapshot semantics                           §9.7–9.10          MVCC, scans, ANALYZE          Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   MVCC visibility                              Chapter 10         scans, uniqueness, vacuum     Consistent                      Exact ordinary predicate
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   FSM is advisory                              Chapter 6          INSERT, recovery              Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Catalog is semantic authority                Chapter 16         binder, DDL, stats            Incomplete                      Physical v1 schemas absent
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Index hit requires heap MVCC                 §8.22              scans, costing                Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   FLOAT64 total semantics                      §§8.5.5, 17.4.3    hash/group/sort/stats         Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Grouping equality differs from = on NULL     §20.9              hash agg/DISTINCT             Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Statistics never change results              §34.1              estimator/optimizer           Contradicted                    A-002
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Required rows is cost only                   §38.16             memo/executor                 Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Capability gating                            §22.4.1            optimizer                     Consistent                      Strong
  ───────────────────────────────────────────  ─────────────────  ────────────────────────────  ──────────────────────────────  ────────────────────────────────
   Committed physical files remain              DDL/recovery       file lifecycle                Incomplete                      A-003
   reopenable

  ## G. Subsystem completeness scorecard

   Subsystem           Semantics    Identity       Ownership    Concurrency       Persistence    Failure        Recovery            Clarity     Verdict
  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━  ━━━━━━━━━━━━━  ━━━━━━━━━━━  ━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━  ━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━  ━━━━━━━━━━━━━━━━━
   Files/storage       Complete     Complete       Complete     Mostly            Incomplete     Incomplete     Partial             Good        UNDERSPECIFIED
                                                                complete
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Pages/buffer        Partial      Complete       Complete     Incomplete        Complete       Incomplete     Partial             Good        UNDERSPECIFIED
   model                                                                          locally                                           locally
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Heap                Complete     Complete       Complete     Mostly            Complete       Mostly         Complete            Strong      COMPLETE WITH
                                                                complete                         complete                                       CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   FSM                 Complete     Complete       Complete     Advisory          Complete       Complete       Complete            Strong      COMPLETE
                                                                freedom
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Tuples/types        Complete     Complete       Complete     N/A locally       Complete       Complete       Complete            Strong      COMPLETE
   storage
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   B+ tree             Mostly       Complete       Complete     Strong            Complete       MTR failure    Strong              Strong      COMPLETE WITH
                       complete                                                                  gap                                            CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Transactions        Mostly       Complete       Complete     Strong            Status gap     Incomplete     Partial             Strong      UNDERSPECIFIED
                       complete
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   MVCC                Complete     Complete       Complete     Complete          Complete       Complete       Complete            Strong      COMPLETE
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   WAL                 Mostly       Complete       Complete     Mostly            Strong         Incomplete     Strong              Strong      UNDERSPECIFIED
                       complete                                 complete
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Checkpoints/        Mostly       Complete       Complete     Mostly            Strong         Status-FPI     Strong otherwise    Strong      INCORRECT
   recovery            complete                                 complete                         defect
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Vacuum              Mostly       Complete       Complete     Partial           Complete       Mostly         Strong              Strong      COMPLETE WITH
                       complete                                                                  complete                                       CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Catalog/            Partial      Strong         Strong       Strong            Incomplete     Partial        Partial             Good        UNDERSPECIFIED
   bootstrap                                                                      schema
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   DDL                 Mostly       Strong         Strong       Strong            Namespace      Partial        Partial             Strong      INCORRECT
                       complete                                                   gap
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Parser/binder       Mostly       Strong         Strong       Query-local       N/A            Mostly         N/A                 Good        UNDERSPECIFIED
                       complete                                                                  complete
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Logical plans       Complete     Strong         Strong       Immutable         N/A            Complete       N/A                 Strong      COMPLETE
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Executor            Mostly       Strong         Strong       Strong            Temporary      Error-state    N/A                 Strong      COMPLETE WITH
                       complete                                 baseline          only           gap                                            CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Query memory/       Complete     Query-local    Strong       Mostly            Temporary      Mostly         Cleanup only        Strong      COMPLETE WITH
   spill                                                        complete                         complete                                       CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Statistics          Mostly       Strong         Strong       Mostly            Strong         Tolerance      Rebuildable         Strong      COMPLETE WITH
                       complete                                 complete          format         gap                                            CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   ANALYZE             Mostly       Strong         Strong       Partial DDL       Complete       Complete       MVCC/rebuildable    Strong      COMPLETE WITH
                       complete                                 interaction                      locally                                        CLARIFICATION
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Cardinality         Incorrect    N/A            Optimizer    N/A               N/A            N/A            N/A                 Detailed    INCORRECT
   estimation          emptiness
                       rule
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Cost model          Complete     N/A            Optimizer    N/A               Config only    Complete       N/A                 Strong      COMPLETE
  ──────────────────  ───────────  ─────────────  ───────────  ────────────────  ─────────────  ─────────────  ──────────────────  ──────────  ─────────────────
   Optimizer/memo      Complete     Strong         Strong       Planning-local    N/A            Complete       N/A                 Strong      COMPLETE

  ## H. State-machine audit

   State machine                       Reconstructable?    Result
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   File lifecycle                      No                  Creation, namespace durability, publication, orphan cleanup, and unlink are not one complete crash-
                                                           safe protocol
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Page lifecycle                      No                  Resident load/flush/evict/error states are missing
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Heap slot lifecycle                 Yes                 NORMAL -> DEAD -> UNUSED -> NORMAL and grace conditions are coherent
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Transaction lifecycle               Mostly              Runtime states are clear; general error transitions and post-durable-commit failures are not
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Tuple-version lifecycle             Yes                 Creation, supersession, abort visibility, vacuum, and chain splicing are coherent
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   WAL/checkpoint lifecycle            Mostly              Record/checkpoint protocols are detailed; status-page FPI and append-failure handling are defective/
                                                           incomplete
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Vacuum/reuse lifecycle              Mostly              RID grace and chain splicing are clear; maintenance serialization/status-reference scope need
                                                           clarification
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Catalog/DDL lifecycle               No                  Logical publication is clear, but exact system schemas and durable physical-file publication are
                                                           incomplete
  ──────────────────────────────────  ──────────────────  ──────────────────────────────────────────────────────────────────────────────────────────────────────
   Statistics publication lifecycle    Mostly              Complete-generation publication is strong; DROP/reclamation edge cases remain

  The exact MVCC visibility predicate can be written from Chapters 9–10 without guessing:

  - creator: frozen is visible; self is visible iff cmin < command_id; another creator must be committed, below xmax, and absent from active;
  - deleter: absent/aborted/in-progress/too-new leaves the tuple visible; self deletion takes effect iff cmax < command_id; an old committed external deleter
    makes it invisible.

  ## I. Cross-subsystem consistency

  - INSERT: coherent through tuple encoding, unique locking, heap redo, index MTR, commit, and abort visibility. It is weakened by the exact unique-check gap,
    general statement-error policy, and durable object-file gap for newly created tables.

  - UPDATE: target-spool Halloween protection, old/new RID versioning, conflict revalidation, and index retention are coherent.
  - DELETE: logical xmax, retained index entries, and later vacuum cleanup are coherent.
  - Index scan: bounds → forward cursor → RID → read epoch → heap MVCC → vector output is coherent.
  - DDL: catalog MVCC/cache publication is coherent; physical file durability before committed publication is not.
  - ANALYZE: snapshot collection and complete-version publication are coherent; concurrent DROP/current-cache publication needs a rule.
  - Recovery: page-image/MTR redo and loser-abort semantics are coherent except for transaction-status page reconstruction.
  - Vacuum: index cleanup before DEAD, grace before UNUSED, and chain splicing prevent RID ABA. Concurrent maintenance ownership needs clarification.
  - Optimizer → executor: capability gating, properties, required rows, and final plan validation are coherent. Statistics-derived provable emptiness is the one
    correctness-breaking exception.

  ## J. Crash-consistency audit

  Crash outcomes are not uniquely defined at these points:

  1. A transaction-status page first becomes dirty from COMMIT/ABORT without a full image.
  2. A committed CREATE references a file whose directory entry was not durably synchronized.
  3. A durable commit resides in a newly created WAL segment whose namespace entry is lost.
  4. B+ runtime bytes have been changed but BTREE_MTR append fails without a process crash.
  5. A raw page file is extended but PAGE_INIT append fails.
  6. COMMIT WAL is durable but status-page installation or terminal publication encounters an I/O/runtime failure.
  7. Database creation synchronizes component files but crashes before the database-root namespace state is durably established.

  Other major crash paths are well defined:

  - heap insert/update/delete redo;
  - complete versus torn B+ MTR;
  - checkpoint END/control publication;
  - crash losers;
  - vacuum index cleanup before DEAD;
  - persistent DEAD with lost process epochs;
  - ANALYZE partial generations;
  - DROP delayed physical retirement.

  ## K. Concurrency audit

  Insufficiently defined interactions:

   Pair                                              Gap
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Concurrent fetches of same page                   Single-loader/waiter publication not defined
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   Eviction vs fetch                                 Page-table/victim linearization not defined
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   Flush vs eviction/reassignment                    Frame ownership during I/O not defined
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   Flush failure vs waiters                          Retry/dirty/resident state not defined
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   VACUUM vs VACUUM                                  Relation-maintenance serialization not explicit
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   ANALYZE vs DROP                                   Commit-time stats publication/current-object rule missing
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   MTR mutation vs WAL append failure                Runtime recovery path unspecified
  ────────────────────────────────────────────────  ───────────────────────────────────────────────────────────
   Terminal status page/cache vs snapshot capture    “cache” identity needs clarification

  Sufficiently defined interactions include SELECT/DML via MVCC, SELECT/vacuum via snapshots plus read epochs, DML/CREATE INDEX through writer gates, concurrent
  writers through logical locks, index scan/split through latch coupling, and checkpoint/normal execution through fuzzy checkpoint and DPT semantics.

  ## L. SQL semantic audit

  - NULL/3VL: strong and consistent across comparison, Boolean operators, filters, joins, CASE, IN, NOT IN, and estimator truth triples.
  - NaN: consistently canonical-equivalent for SQL equality/grouping/index/statistics; sorts after +infinity.
  - Signed zero: tuple storage preserves bits; SQL equality/hash/index/grouping treats -0.0 and +0.0 as equivalent.
  - GROUP BY/DISTINCT: explicitly use grouping equivalence, including one NULL class.
  - Ordering: direction, NULL positioning, binary collation, and FLOAT total order agree.
  - VARCHAR: consistently opaque/binary bytes in v1.
  - DATE/TIMESTAMP: epoch/unit are locked and agree with physical scalars.
  - Numeric promotion: widening hierarchy is clear.
  - Aggregate result types: clear for COUNT/SUM/MIN/MAX/AVG.
  - IN/NOT IN: correct 3VL behavior is specified.
  - Remaining semantic gaps: literal typing, complete arithmetic operator registry, integer division/remainder details, exact explicit-cast registry, and
    conversion formatting/parsing.

  ## M. Optimizer semantic audit

  Strong contracts:

  - estimates ordinarily affect performance only;
  - physical algorithms are capability gated;
  - ordering properties exactly include slot, direction, NULL order, and collation;
  - reverse B+ order is not advertised;
  - LEFT hash join preserves the logical left side;
  - required rows is a cost objective, not a semantic LIMIT;
  - memo dominance retains interesting-order and low-startup alternatives;
  - ties use deterministic structural keys;
  - final physical plans are validated.

  Defect:

  - statistics-based min/max “proof” violates the estimates-only rule and can change results. After A-002 is corrected, the optimizer architecture is
    semantically safe.

  ## N. Persisted-contract semantic audit

  Most persisted structures have canonical meaning, validation, versioning, and ownership:

  - Common page, heap, tuple, VARCHAR, FSM, B+ node/free/superblock, RID, WAL, control, bootstrap locator, persisted scalar, default blob, and statistics
    payload byte layouts are precise.

  - Mutation protocols are strongest for heap/B+ pages and control slots.
  - Version strictness is generally good.

  Incomplete persisted semantics:

  1. transaction-status dirty-interval/FPI protocol;
  2. exact v1 system-relation schemas;
  3. superblock unknown-flag handling;
  4. REDIRECT_RESERVED reader validity;
  5. exact statistics mass tolerance/MCV uniqueness;
  6. durable namespace publication for files/WAL;
  7. failure handling for partially completed append/MTR publication.

  ## O. Two-implementers ambiguity list

  Two competent implementations could currently diverge materially on:

  1. transaction-status page FPI generation;
  2. whether stale min/max can remove an access path entirely;
  3. directory synchronization and committed file publication;
  4. BufferPool frame states and failed-load cleanup;
  5. whether concurrent page misses share one loader;
  6. MTR append-failure rollback versus fatal shutdown;
  7. unpublished append-tail cleanup;
  8. physical schemas of all six catalog relations;
  9. encoding of constraint-definition payloads;
  10. same-transaction/current-command uniqueness conflicts;
  11. same-key UPDATE self-exclusion;
  12. which execution errors abort a statement versus the transaction;
  13. client outcome after durable commit but failed runtime publication;
  14. integer literal typing;
  15. integer division/remainder semantics;
  16. supported explicit casts and their formatting/parsing;
  17. actual v1 scalar function registry;
  18. whether initial uncorrelated subqueries are executable v1 features;
  19. normal-query validation of unsorted/overlapping B+ entries;
  20. B+ free-list cycle validation;
  21. validity of REDIRECT_RESERVED;
  22. validity of unknown superblock flags;
  23. whether persisted StatsVersion TxnIds pin status history;
  24. CommandId and LSN exhaustion handling;
  25. statistics floating-mass validation epsilon;
  26. concurrent VACUUM ownership;
  27. statistics publication after concurrent DROP;
  28. database process-exclusivity enforcement.

  ## P. V1 feature closure matrix

   Feature                        Complete?    Missing semantic link
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Database create/open           No           Durable namespace and open/exclusivity state
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Table create                   No           Durable file publication and exact catalog schemas
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Table scan                     Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   INSERT                         Mostly       Unique truth table and general failure policy
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   UPDATE                         Mostly       Unique self-conflict and general failure policy
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   DELETE                         Mostly       General failure policy
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Index creation                 No           Durable private-file publication
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Index scan                     Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Transaction begin              Mostly       CommandId exhaustion/open failure details
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Commit/abort                   No           Status-page FPI and post-durable failure behavior
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Snapshot visibility            Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Checkpoint                     Mostly       Depends on valid status-page FPI
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Crash recovery                 No           Status page and namespace durability defects
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Vacuum                         Mostly       Concurrent maintenance/status-reference clarification
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Defaults                       Mostly       Cast/literal semantics incomplete
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   NOT NULL/UNIQUE/PRIMARY KEY    Mostly       Exact unique-current-state predicate
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   ANALYZE                        Mostly       DROP/publication edge
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   SELECT                         Mostly       Function/cast/subquery scope ambiguity
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Joins                          Yes          Capability gating is explicit
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   GROUP BY                       Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   DISTINCT                       Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   ORDER BY                       Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   LIMIT/OFFSET                   Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Query memory/spill             Mostly       Transaction effect of spill failure
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Cardinality estimation         No           Statistics-based semantic emptiness defect
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   Cost optimization              Yes          —
  ─────────────────────────────  ───────────  ───────────────────────────────────────────────────────
   EXPLAIN/ANALYZE                Yes          —

  ## Q. Open-question audit

  The statement in Appendix D that no unresolved core v1 question remains is not currently accurate.

  Actual unresolved v1 questions are:

  1. How are transaction-status clean-to-dirty intervals made reconstructible?
  2. What exact filesystem namespace durability is required before successful commit/create acknowledgement?
  3. What is the BufferPool frame/I/O state machine?
  4. What are the exact six catalog schema-version-1 descriptors?
  5. What happens after WAL/MTR/page-publication failure?
  6. What exact current-state predicate enforces UNIQUE?
  7. Which errors abort a statement versus the entire transaction?
  8. What outcome is reported after commit WAL is durable but completion fails?
  9. What exact arithmetic/cast/literal/function surface is v1?
  10. Are initial uncorrelated subqueries executable v1 features?
  11. Which B+ and heap structural invariants are mandatory on ordinary page open?
  12. Does StatsVersion identity participate in transaction-status retention?
  13. What are CommandId/LSN/PageNo exhaustion rules?
  14. How are concurrent maintenance operations serialized?
  15. What are the strict decoder rules for superblock flags and REDIRECT_RESERVED?
  16. What exact tolerance applies to persisted statistics masses?
  17. Is single-process database exclusivity enforced or externally assumed?

  ## R. Architecture organization/maintainability findings

  1. Canonical local formats are generally well organized and cross-referenced.
  2. Chapter summaries frequently repeat invariant lists. They are useful, but revisions must continue treating owning sections as canonical to avoid drift.
  3. Transaction status behavior is split across Chapters 9, 12, 13, and 15; the missing integrated status-page WAL protocol is a direct consequence of that
     distribution.

  4. File durability is split among Chapters 4, 7, 12, 13, 16, and 21 without one persistent-file lifecycle owner.
  5. SQL operator semantics are spread across Chapters 17, 19, 25, 29, and 39 without a closed v1 registry.
  6. Rewrite-provenance wording (R-037, Pass-13, “chapters are migrated”) should not remain in the authoritative architecture.
  7. Appendix D’s “no open questions” claim should be updated only after the substantive decisions above are resolved.

  ## S. Recommended corrections

  Ordered by priority:

  1. Prohibit approximate/stale statistics from establishing semantic emptiness.
      - Change type: architecture semantics/correctness

  2. Define a complete transaction-status WAL/FPI/page-LSN/recLSN protocol.
      - Change type: architecture semantics and crash recovery

  3. Define durable filesystem namespace publication for database files and WAL segments.
      - Change type: architecture semantics and durability

  4. Define the BufferPool resident-frame state machine and all I/O failure transitions.
      - Change type: architecture semantics/concurrency
      - This is required before BufferPool implementation.

  5. Define exact catalog schema-version-1 descriptors and constraint payloads.
      - Change type: persistent compatibility

  6. Define MTR and append-publication failure atomicity.
      - Change type: concurrency/recovery

  7. Define the exact unique-check truth table, including self/current-command and excluded-target cases.
      - Change type: SQL/transaction semantics

  8. Add a statement-error/transaction-state matrix and post-durable-commit failure contract.
      - Change type: transaction semantics

  9. Lock the v1 scalar literal/operator/cast/function registry.
      - Change type: SQL semantics and default persistence

  10. Explicitly include or defer uncorrelated subquery execution.
      - Change type: v1 feature semantics

  11. Strengthen normal-operation structural validation for heap/B+ pages and free lists.
      - Change type: corruption semantics

  12. Clarify StatsVersion versus status reclamation.
      - Change type: reclamation semantics

  13. Add CommandId, LSN, and PageNo exhaustion rules.
      - Change type: lifecycle correctness

  14. Define maintenance-operation serialization/publication interactions.
      - Change type: concurrency semantics

  15. Make superblock flags, REDIRECT_RESERVED, and statistics tolerance strictly canonical.
      - Change type: persistent compatibility

  16. Remove provenance wording and stale migration language.
      - Change type: clarification/document organization only

  ## T. Final readiness statement

  1. Is ARCHITECTURE.md sufficient to implement complete intended v1 without inventing core semantics?
     No.

  2. Are all persisted compatibility decisions sufficiently locked?
     No. System-catalog schemas, strict reserved-state handling, and statistics validation remain incomplete.

  3. Are transaction, MVCC, WAL, recovery, vacuum, and RID reuse mutually coherent?
     Mostly, but not fully. MVCC/vacuum/RID reuse are coherent; transaction-status FPI and failure publication are not.

  4. Are SQL semantics coherent across binder, executor, index, statistics, and optimizer?
     Mostly. NULL, FLOAT64, grouping, ordering, and hash semantics agree. Cast/literal/operator coverage is incomplete, and statistics-derived emptiness is
     incorrect.

  5. Are optimizer transformations and properties semantically safe?
     Yes, except for statistics-derived is_provably_empty.

  6. Are lifecycle, ownership, concurrency, and failure rules sufficiently explicit?
     No. Buffer frames, durable file publication, MTR failure, maintenance coordination, and statement errors require decisions.

  7. Are architectural changes required before Implementation Phase 2?
     Yes. At minimum M-001 must be resolved before BufferPool implementation. A-001 and A-003 also affect the recovery/durability design that BufferPool must
     eventually enforce.

  8. If only later-subsystem clarification were required, would it block BufferPool specifically?
     This is not only later-subsystem clarification. The missing BufferPool state machine directly blocks BufferPool. Catalog/type/subquery corrections do not
     independently block initial BufferPool work, but the durability/FPI contracts influence its design.

  9. Can the architecture be frozen as the v1 semantic specification after the reported corrections?
     Yes. The architecture’s core design is viable; the defects appear locally correctable without replacing the overall system model. Until then, it should not
     be frozen as a complete v1 semantic specification.

  Implementation Phase 2 remains not started and requires explicit project-owner authorization.