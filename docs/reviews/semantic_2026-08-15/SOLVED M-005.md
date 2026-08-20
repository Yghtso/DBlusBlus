• M-005 is resolved in docs/ARCHITECTURE.md:18944.

  1. Runtime states: ACTIVE, MUST_ABORT, COMMITTING, COMMITTED, ABORTING, and ABORTED. Runtime-only states require no new persisted status codes.
  2. Statement outcomes: SUCCESS, FAILED_TRANSACTION_REMAINS_ACTIVE, FAILED_TRANSACTION_MUST_ABORT, COMMIT_OUTCOME_UNCERTAIN, and DATABASE_NONCONTINUABLE.
  3. First persistent statement write: the first transaction-owned logical mutation published as valid WAL-backed database state. LSN reservation, temporary resources, rolled-back M-003
     provisional mutations, structural allocation artifacts, and pre-catalog DDL files do not qualify.

  4. Error matrix: recoverable errors before that boundary leave explicit transactions active; after it they require MUST_ABORT. Independently transaction-fatal errors always abort; storage-
     incoherent failures make the database noncontinuable.

  5. Read-only/pre-write errors ordinarily leave an explicit transaction ACTIVE, absent corruption, invariant failure, or another fatal classification.
  6. Multirow DML: if rows 1–4 publish and row 5 fails, the statement fails, COMMIT is forbidden, and the transaction automatically aborts. Earlier rows remain aborted physical garbage.
  7. Local rollback exception: an exactly restored M-003 provisional operation does not itself cross the boundary. Earlier published effects from the same statement still require abort.
  8. Write-conflict retry: transparent retry is allowed only before publication and while all attempt-local state/output is discardable. Same-TxnId retry after publication is forbidden.
  9. CommandIds are monotonic and never reused. Every admitted successful or failed statement consumes its CommandId; a retry keeps the logical statement’s CommandId but obtains a fresh READ
     COMMITTED snapshot.

  10. NOT NULL, UNIQUE, and PRIMARY KEY violations follow the publication boundary. M-004’s exact UNIQUE predicate remains unresolved and unchanged.
  11. Partial UPDATE/DELETE failure after publishing a new version, xmax/cmax, or index effect requires automatic abort.
  12. RETURNING is buffered through statement success. Autocommit output remains unexposed until implicit COMMIT runtime publication and required cleanup complete.
  13. OOM and spill failures are classified by whether the statement already published a database write; temporary spill state is not itself persistent transaction state.
  14. ANALYZE failure before statistics-row publication may leave the transaction active. Failure after any sys_statistics row publication requires abort. Post-commit cache failure cannot alter
     COMMITTED.

  15. Pre-catalog DDL files and consumed IDs are cleanup/allocation artifacts. Catalog publication crosses the boundary; later DDL failure requires abort. A-003 namespace uncertainty remains
     governed by its existing orphan/noncontinuable rules.

  16. Known WAL append failure with exact M-003 restoration follows the statement boundary. Append uncertainty is database-noncontinuable. Ordinary WAL-flush failure retains legal dirty state;
     COMMIT flush failures retain COMMITTING and retry or escalate to recovery-required uncertainty.

  17. COMMIT now has exact C0–C6 stages in docs/ARCHITECTURE.md:9098: preconditions, authorizing append/status installation, WAL durability, runtime terminal publication, coherent cache/lock
     cleanup, then acknowledgement.

  18. The irreversible durable-commit point is durable_lsn >= commit_lsn. Afterward the transaction is semantically COMMITTED forever.
  19. Post-durable failures cannot produce ABORTED. Unsafe runtime-publication failure makes the database noncontinuable; the client observes outcome uncertainty if success was not delivered.
  20. Successful COMMIT acknowledgement occurs only after terminal cache/registry publication and required coherent lock/cache cleanup.
  21. ABORT has exact A0–A4 stages in docs/ARCHITECTURE.md:9132: enter ABORTING, publish abort status when required, publish runtime ABORTED, release resources, then acknowledge/return the
     original error.

  22. V1 automatically aborts a MUST_ABORT transaction. Ordinary statements and COMMIT are forbidden while cleanup is pending; explicit ROLLBACK may only join that cleanup.
  23. Autocommit uses the same matrix but has no continuation: statement success proceeds to COMMIT; failure proceeds to ABORT. V1 does not transparently rerun an aborted transaction.
  24. Connection loss before the authorizing commit append aborts. After that append COMMIT is uncancellable. Loss after durable commit cannot undo COMMITTED.
  25. MUST_ABORT is transaction-fatal while the database remains usable. WAL/publication uncertainty, failed provisional restoration, and runtime ownership incoherence are database/storage-
     noncontinuable.

  26. Transaction locks, writer gates, schema locks, and snapshots remain held through terminal publication. MUST_ABORT alone does not release them.
  27. Catalog/statistics caches never define transaction outcome. They must install, invalidate/bypass, or use a safe fallback; failure to preserve coherent visibility after durable commit makes
     the database noncontinuable but leaves COMMITTED unchanged.

  28. Recovery classifies transactions without a surviving complete COMMIT record as losers/ABORTED. A complete COMMIT record in the valid persisted WAL prefix recovers as COMMITTED, regardless of
     acknowledgement.

  29. The architecture now explicitly forbids partial DML commit, post-publication retry, CommandId reuse, premature RETURNING, early lock release, statements in MUST_ABORT, durable-commit
     rollback, early COMMIT acknowledgement, and cache-defined transaction outcomes.

  30. Sections changed: §§4.7.5, 7.10.4, 7.12.7, 9.4, 9.6, 9.7.2, 9.9, 9.14, 9.16, 12.10.5, 12.12.4, 12.15, 12.18, 13.17, 13.20–13.21, 15.5–15.7, 15.9, 21.2, 21.5, 21.10, 21.15, 21.17.1, 24.10–
     24.11, 31.5, 31.9, 31.12.1, 31.13, 34.15, 34.17, 39.1–39.3, 40.3–40.4, 41.3, and Appendix B.

  31. No persisted format, WAL record, page format, catalog format, or transaction-status encoding changed.
  32. No correctness-relevant M-005 semantic question remains open.
  33. No new unresolved architecture question was introduced.
  34. Production code and tests were untouched. Only docs/ARCHITECTURE.md changed. git diff --check passed; code tests were not run because this was documentation-only architecture work.
  35. Implementation Phase 2 was NOT entered.