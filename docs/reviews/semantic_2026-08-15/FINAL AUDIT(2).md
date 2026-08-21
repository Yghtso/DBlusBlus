• # SA-001 resolution

  1. Canonical model: SA-001 was real. V1 now uses a hybrid protocol: structurally unsafe same-transaction transitions are
     rejected before waiting; every other transaction-level blocking dependency enters one unified database-local wait-for
     graph.

  2. SchemaLock: One database-global exclusive, reentrant, transaction-owned gate. Pre-publication failed attempts may
     release it; after DDL/catalog publication it remains held through terminal publication and releases at C5/A3.

  3. TableWriterGate: Per-table SHARED_WRITER for persistent DML and EXCLUSIVE_DDL for CREATE INDEX/DROP. Granted DML
     ownership remains through terminal publication. Exclusive ownership subsumes later same-transaction shared ownership.

  4. STATS_PUBLISH: Scoped by TableId, schema version, and immutable column/index manifest. Compatible statistics claims are
     shared. After the first statistics row publishes, ownership remains through terminal publication. MANIFEST_CHANGE is now
     the corresponding exclusive object-publication mode for CREATE INDEX, DROP INDEX, and DROP TABLE.

  5. Acquisition order: DML uses shared writer gate before tuple/UNIQUE locks. DDL uses SchemaLock, target writer gates in
     ascending TableId, private construction, manifest-change scopes in deterministic identity order, then publication
     latches. ANALYZE publication acquires table then ascending-index statistics claims.

  6. Cross-statement rule: Retaining an earlier statement’s gate does not make later reverse-order acquisition implicitly
     illegal. Legal waits use the unified graph. Two same-transaction inversions are proactively rejected.

  7. DML → DDL: UPDATE A; CREATE TABLE B is legal and deadlock-managed. DDL requiring exclusive ownership of the same table,
     such as UPDATE A; CREATE INDEX A, is rejected before waiting.

  8. ANALYZE → DDL: Unrelated DDL remains legal and deadlock-managed. Manifest-changing DDL affecting an already-published
     generation in the same transaction is rejected before waiting.

  9. DDL → DML: Legal. Existing same-table exclusive writer ownership subsumes the shared request; different-table conflicts
     use the unified graph.

  10. Same-table upgrade: Shared-to-exclusive TableWriterGate upgrade remains unsupported and is deterministically rejected
     before waiting or publication.

  11. Wait-for edges: SchemaLock, both TableWriterGate modes, STATS_PUBLISH, MANIFEST_CHANGE, TUPLE_WRITE, UNIQUE_KEY, and
     elected fairness-queue dependencies all contribute edges before sleeping.

  12. Logical-lock integration: Gate dependencies and tuple/UNIQUE dependencies share one graph. Page latches, BufferPool
     transitions, read epochs, and physical retirement drains remain outside it.

  13. Victim policy: Cycle detection occurs synchronously when edges are installed or replaced. The highest normal TxnId in
     the cyclic component is the deterministic victim.

  14. Victim outcome: DEADLOCK_DETECTED is transaction-fatal under M-005. The victim enters MUST_ABORT and follows normal
     abort processing; ordinary deadlock does not make the database noncontinuable.

  15. Release timing: Commit releases transaction-owned gates at C5 after COMMITTED publication. Abort releases them at A3
     after ABORTED publication. Victim selection alone releases nothing.

  16. Wakeup rule: Every waiter rebuilds dependencies and revalidates current catalog identity, descriptor, manifest,
     RETIRING state, target/key state, and other resource-specific assumptions.

  17. DROP/RETIRING: DROP obtains MANIFEST_CHANGE before RETIRING publication. Deadlock resolution never authorizes early
     unlink. Physical retirement still waits for descriptors, maintenance claims, guards, frames, and file owners to drain.

  18. Maintenance owners: VACUUM, object-use claims, status guards, checkpoints, and retirement cleanup remain outside the
     transaction graph because their canonical protocols do not wait back on transaction gates. Maintenance using a real
     catalog transaction must use that transaction’s TxnId in the unified graph.

  19. Lifecycle: DRAINING admits no new ordinary statements or gates but permits terminal cleanup. NONCONTINUABLE admits no
     new ordinary synchronization or mutation.

  20. Operation lock-order table: The canonical table covers SELECT, DML, ANALYZE scan/publication, CREATE TABLE, CREATE
     INDEX, DROP TABLE/INDEX, VACUUM publication units, COMMIT, and ABORT, including order, lifetime, graph participation,
     and wakeup validation.

  21. Retained-ownership table: Every combination of retained SchemaLock, shared/exclusive TableWriterGate, STATS_PUBLISH,
     and MANIFEST_CHANGE is classified as ALLOW, WAIT + DEADLOCK GRAPH, or REJECT BEFORE WAIT.

  22. Adversarial timelines:
      - UPDATE/CREATE INDEX/CREATE TABLE cycle: unified graph; highest TxnId aborts.
      - ANALYZE/DDL/SchemaLock cycle: unified graph; highest TxnId aborts.
      - Cross-table DML followed by opposing DDL: unified graph; highest TxnId aborts.
      - Two ANALYZEs followed by cross-object DDL: unified graph; same-scope self-DDL is proactively rejected.
      - Same-table writer upgrade: rejected before waiting.
      - Autocommit UPDATE versus CREATE INDEX: DDL waits for terminal UPDATE, then revalidates.

  23. Completeness argument: Every legal transaction-to-transaction sleep has complete owner/queue edges in one finite graph
     and triggers cycle detection before sleeping. Excluded cases either fail before waiting or belong to nontransaction
     protocols that cannot wait back on transaction gates.

  24. Forbidden implementations: The architecture now explicitly forbids partial/separate wait graphs, tuple-lock-only
     detection, reliance on per-statement ordering or timeout victims, omitted fairness dependencies, early ownership
     release, continuation without wakeup revalidation, deadlock-to-NONCONTINUABLE escalation, waiting under short physical
     ownership, and premature retirement after victim selection.

  25. Architecture sections changed: The canonical owner is docs/ARCHITECTURE.md:8086. Concise synchronization was added to
     §§9.4, 9.14, 11.1, 11.14–11.15, 14.17.1, 15.1.1, 15.5–15.6, 21.2, 21.8.2, 21.9, 21.17.1, 39.1.3, 39.1.8, and 41.3.

  26. Persisted formats: No persisted format, field, byte layout, numeric code, or version changed.
  27. SQL capability: No statement form was removed. Two previously underspecified same-transaction sequencing cases now
     receive deterministic pre-wait rejection.

  28. Remaining SA-001 questions: None.
  29. Prerequisite dependencies: None discovered.
  30. New architecture questions: None introduced.
  31. Production code/tests: Untouched. Only docs/ARCHITECTURE.md was modified. git diff --check passes. The pre-existing
     untracked audit file remains untouched.

  32. Phase boundary: Implementation Phase 2 was not entered and remains unauthorized.