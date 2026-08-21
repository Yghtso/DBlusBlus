• Resolved A-003 with a canonical staged filesystem namespace protocol.

  1. Chosen namespace durability model

  One database_root owns:

  database.control
  catalog.dat
  txn_status.dat
  pending/
  wal/
  table_<table_id>.heap
  table_<table_id>.fsm
  index_<index_id>.btree

  The process retains directory descriptors for the root, pending/, and wal/. Managed paths use directory-relative, no-follow access.

  The canonical contract begins at docs/ARCHITECTURE.md:546.

  2. Canonical file-creation sequence

  DDL object files are created under exact private names in pending/:

  txn_<txn_id>_file_<file_id>.<kind>

  Creation requires:

  1. Exclusive creation.
  2. Complete initial contents and length.
  3. fdatasync(file).
  4. fsync(pending_directory).

  Before catalog commitment:

  1. Complete and flush the private build under WAL-before-data.
  2. fdatasync every required file.
  3. Rename each file without replacement to its deterministic final name.
  4. fsync(database_root).
  5. fsync(pending_directory).
  6. Directory synchronization rule

  - fdatasync(file) makes regular-file data and retrieval-critical metadata durable.
  - It does not durably publish create, rename, or unlink.
  - Every affected parent directory requires fsync.
  - Cross-directory rename requires both source and destination directory synchronization.
  - close is not a durability primitive.

  4. Private/public naming rule

  The file state machine is now explicit:

  ABSENT
  -> PRIVATE_DURABLE
  -> FINAL_DURABLE_UNCOMMITTED
  -> CATALOG_COMMITTED
  -> RETIRED_LINKED
  -> UNLINK_PENDING
  -> ABSENT_DURABLE

  Abort paths enter ORPHAN_LINKED.

  Runtime rename is atomic for lookup. Durable physical publication occurs only after both directory synchronizations. Semantic publication still occurs through
  committed catalog state.

  5. DDL publication rule

  CREATE catalog rows cannot become commit-eligible until every required heap, FSM, and B+ file is durably final-name-published.

  The generic COMMIT protocol now explicitly requires this DDL prerequisite. Acknowledged committed catalog state therefore cannot reference a namespace entry
  that may disappear after crash.

  6. WAL-segment rule

  WAL segments use direct final names:

  16 lowercase hexadecimal digits + ".wal"

  Each new segment:

  1. Is created exclusively at the next contiguous index.
  2. Is truncated to exactly 64 MiB with no separate header.
  3. Is fdatasynced.
  4. Has wal/ immediately fsynced.
  5. Only then becomes eligible for WAL writes or durability requests.
  6. Requires another fdatasync after record writes before durable_lsn advances.

  A flush spanning segments synchronizes every segment containing unsynchronized bytes. The protocol is in docs/ARCHITECTURE.md:6264.

  7. Bootstrap durability

  Database creation uses a sibling staging root:

  <final-name>.dblusblus-creating

  All control, catalog, status, system-relation, directory, and initial-WAL contents are synchronized inside it. The complete staging root is then renamed
  without replacement to the final database name, followed by fsync of the external parent directory.

  Creation reports success only after that final parent sync.

  8. Unlink and retirement

  DROP remains catalog-transactional and does not unlink immediately.

  After snapshot, descriptor, BufferPool, and file-owner retirement gates:

  1. Prevent new opens and drain existing ownership.
  2. unlinkat the managed name.
  3. fsync its parent directory.
  4. Only then classify removal as durable.

  WAL recycling and orphan cleanup use the same unlink-plus-directory-sync rule.

  9. Namespace failure semantics

  A file or directory synchronization failure is an I/O/durability failure, never successful publication.

  - File sync success followed by directory-sync failure leaves publication uncertain and forbids terminal CREATE commit.
  - Durable final name followed by catalog/commit failure leaves an orphan.
  - Failed unlink synchronization leaves retirement pending and retryable.
  - WAL directory-sync failure prevents durable_lsn from entering the new segment.

  The separate general post-durable-commit error policy was not resolved or changed.

  10. Orphan recovery

  After restart:

  - Every exact pending/ entry is uncommitted and orphaned.
  - Final object files are classified only after recovery reconstructs committed catalog ownership.
  - Unowned managed final files are orphans.
  - Missing files required by bootstrap or committed catalog state are corruption/open failure.
  - Unknown unrelated names are not deleted.
  - Missing WAL targets may be skipped only after proving no committed/bootstrap owner requires them.

  11. Crash-point reasoning

  Ordinary CREATE:

  - Before durable final publication: survivors are private/orphan candidates; commit is forbidden.
  - After durable final publication but before catalog WAL: final orphan.
  - After catalog WAL but before durable commit: loser transaction and orphan.
  - After durable commit: recovery finds committed catalog state and the already-durable final files.
  - After acknowledgement: metadata and required filenames survive.

  WAL rollover:

  - Before directory sync: segment may survive or disappear, but no durability acknowledgement depends on it.
  - After directory sync before writes: durable empty segment.
  - After writes before fdatasync: only the surviving valid prefix is recoverable.
  - After fdatasync before waiter wake: WAL is durable but unacknowledged.
  - After durable_lsn publication: both segment name and acknowledged bytes survive.

  Retirement:

  - Before unlink: retry later.
  - After unlink before directory sync: name may reappear; cleanup retries.
  - After directory sync: absence is durable.

  12. Sections changed

  - §2.5
  - §4.1, §4.7.1–§4.7.8, §4.13
  - §7.3, §7.3.1, §7.4.3, §7.4.8, §7.13
  - §12.1, §12.2, new §12.2.1, §12.13, §12.15, §12.18
  - §13.10, §13.11, §13.13.1, §13.19–§13.21
  - §15.5
  - §16.9.5, §16.11
  - §21.5, §21.6.2, §21.8.2, §21.9, §21.20
  - Appendix B

  All architecture section references resolve, numbered headings remain unique, and git diff --check passes.

  13. Persisted formats

  No binary page, WAL-record, control-file, catalog, identifier, numeric code, or format version changed.

  The filesystem namespace contract was newly locked, including exact private names and WAL segment basenames.

  14. Architecture questions

  No new architecture question was introduced. M-001 and the separate general post-durable-commit error-policy issue remain untouched.

  15. Production scope

  Production code, tests, PROJECT_STATE, DEVELOPMENT, VERIFICATION, and devlogs were untouched. The pre-existing untracked semantic-review directory was also
  untouched.

  16. Phase boundary

  Implementation Phase 2 was not entered.