• Implemented the Phase 1 foundational identifier layer only.

Files changed:

- src/common/types.h:10
  - Added all requested fixed-width aliases.
  - Added named sentinel/reserved constants.
  - Added PageId and Rid.
  - Added defaulted comparisons and std::hash<PageId>.

- tests/smoke_test.cpp:12
  - Added focused width, sentinel, construction, comparison, hashing, and field-semantics tests.

Architecture sections used:

- §54 — Fundamental Identifier Types
- §55 — PageId
- §56 — RID
- §113 — numeric lexicographic RID ordering
- §182 — reserved transaction IDs
- §186 — initial CommandId

Checks run:

- Six focused identifier/PageId/Rid tests: passed.
- Full clang-debug build and CTest suite: 7/7 passed.
- Fresh clang-tidy build over core and tests: passed without warnings after corrections.
- clang-format --dry-run --Werror: passed.
- git diff --check: passed.

Assumptions:

- Used aliases rather than strong wrapper classes because §54 specifies conceptual using declarations.
- Default PageId and Rid use their invalid sentinel fields.
- Added only constants explicitly defined by the contract. No speculative invalid values were created for CommandId, TableId, IndexId, or SchemaVer; command ID zero is valid.
- Defaulted comparisons provide field-wise lexicographic ordering.

No serialization, storage, WAL, page-header, file, or buffer-pool functionality was added.
