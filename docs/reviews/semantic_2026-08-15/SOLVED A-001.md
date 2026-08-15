• Resolved A-001 using the ordinary-page FPI model.

  1. Chosen model

  TXN_STATUS pages now obey the universal clean-to-dirty full-page-image invariant:

  - Existing page: system PAGE_IMAGE.
  - Newly allocated page: system PAGE_INIT.
  - TXN_COMMIT/TXN_ABORT remain separate semantic terminal records.

  The canonical protocol is in docs/ARCHITECTURE.md:6392.

  2. Why it is correct

  The full image captures the pre-terminal page state and provides physical torn-page reconstruction. The later terminal record alone establishes the new
  COMMITTED or ABORTED outcome.

  This prevents a page image from publishing a pending outcome without its semantic WAL record, while still allowing later images to preserve previously
  established terminal bits after older WAL becomes recyclable.

  3. Alternative rejected

  Exempting TXN_STATUS would require either:

  - retaining every terminal record for every unreclaimed status page, or
  - defining a separate status-page snapshot and retention protocol.

  That would duplicate the existing FPI machinery and complicate WAL recycling. No exemption was adopted.

  4. Sections changed

  - §9.12
  - §9.14.2–§9.14.3
  - §12.7.2–§12.7.3
  - New §12.10.5–§12.10.5.4
  - §12.16
  - §12.18
  - §13.12
  - §13.13.2
  - §13.14–§13.15
  - §13.17
  - §13.21
  - §15.5–§15.6

  5. Clean-to-dirty protocol

  Under the status-page write latch and checkpoint/DPT transition synchronization:

  1. Determine whether an FPI is required.
  2. Append a system pre-terminal PAGE_IMAGE at F if required.
  3. Append the semantic terminal record at T, where F < T.
  4. Install the two-bit terminal state.
  5. Set resident page_lsn = T.
  6. Publish dirty, rec_lsn, and FPI-epoch metadata atomically relative to checkpoint capture.
  7. page_lsn

  - Full WAL image embeds page_lsn = F.
  - After terminal mutation, resident page has page_lsn = T.
  - During redo, a page may temporarily have image LSN F before terminal record T is applied.

  7. rec_lsn

  - New clean-to-dirty interval: rec_lsn = F.
  - New page: rec_lsn = PAGE_INIT.lsn.
  - Further updates while dirty preserve the original rec_lsn.
  - Successful stable flush resets it to INVALID_LSN.
  - A later clean-to-dirty interval requires a new image.

  8. Terminal-record ordering

  The image is a system record:

  txn_id = 0
  prev_txn_lsn = 0

  The later terminal record remains on the user transaction’s WAL chain and points to the previous user WAL record, not the image.

  COMMIT waits for durability through T before runtime terminal publication. ABORT need not synchronously flush, but WAL-before-data prevents its status page
  from being written before T is durable.

  9. Crash-point outcomes

  - Before FPI: no mutation; unresolved transaction becomes a loser.
  - After FPI, before terminal record: image restores pre-terminal state; loser resolution aborts.
  - After terminal append, before flush: the page cannot reach disk without WAL durability; recovery uses the terminal record only if it survived validly.
  - After terminal durability, before page mutation: terminal redo installs the outcome.
  - After mutation, before page flush: image plus terminal redo reconstructs it.
  - Torn status-page write: checksum failure forces reconstruction from the retained image.
  - After successful page flush: trusted page_lsn skips reflected records.
  - After checkpoint/recycling: dirty pages retain their image through DPT rec_lsn; clean pages are durable bases and must emit a new image when dirtied again.

  10. WAL retention

  A dirty status page’s DPT rec_lsn always identifies PAGE_INIT or PAGE_IMAGE. WAL recycling retains that image and subsequent required terminal records.

  Checkpoint capture now explicitly synchronizes with status-page dirty publication, preventing an FPI from preceding the retention floor while its page is
  omitted as apparently clean.

  11. Persisted formats

  No persisted record format, numeric record type, payload, field width, page format, or version changed.

  12. Architecture questions

  No new architecture question was introduced. The two-implementer test now yields one answer for:

  - FPI trigger
  - image/terminal ordering
  - rec_lsn
  - page_lsn
  - WAL flushing
  - redo order
  - dirty-interval reset
  - WAL recycling

  13. Verification and scope

  - All §x.y references resolve.
  - No duplicate numbered headings were introduced.
  - git diff --check passes.
  - Only docs/ARCHITECTURE.md was modified by this task.
  - The pre-existing untracked semantic-review directory was not modified.
  - Production code, tests, PROJECT_STATE, DEVELOPMENT, VERIFICATION, and devlogs were untouched.

  14. Phase boundary

  Implementation Phase 2 was not entered.