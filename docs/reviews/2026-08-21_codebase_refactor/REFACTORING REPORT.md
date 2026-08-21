• ## 1. Completed target source tree

  src/
  ├── CMakeLists.txt
  ├── main.cpp
  ├── common/
  │   ├── crc32c.cpp
  │   ├── crc32c.h
  │   ├── encoding.h
  │   └── types.h
  └── storage/
      ├── disk/
      │   ├── disk_manager.cpp
      │   └── disk_manager.h
      ├── file/
      │   ├── file_superblock.cpp
      │   ├── file_superblock.h
      │   ├── page_file.cpp
      │   └── page_file.h
      ├── page/
      │   ├── page.cpp
      │   ├── page.h
      │   └── page_header.h
      ├── heap/
      │   ├── fsm_candidate_index.cpp
      │   ├── fsm_candidate_index.h
      │   ├── fsm_page.cpp
      │   ├── fsm_page.h
      │   ├── heap_page.cpp
      │   ├── heap_page.h
      │   ├── heap_page_format.cpp
      │   └── heap_page_format.h
      └── tuple/
          ├── tuple_codec.cpp
          ├── tuple_codec.h
          ├── tuple_header.cpp
          ├── tuple_header.h
          ├── tuple_layout.cpp
          └── tuple_layout.h

  ## 2. Completed target test tree

  tests/
  ├── CMakeLists.txt
  ├── smoke_test.cpp
  ├── common/
  │   ├── crc32c_test.cpp
  │   ├── encoding_test.cpp
  │   └── types_test.cpp
  └── storage/
      ├── disk/
      │   └── disk_manager_test.cpp
      ├── file/
      │   ├── file_superblock_test.cpp
      │   └── page_file_test.cpp
      ├── page/
      │   ├── page_header_test.cpp
      │   └── page_test.cpp
      ├── heap/
      │   ├── fsm_candidate_index_test.cpp
      │   ├── fsm_page_test.cpp
      │   ├── heap_page_format_test.cpp
      │   ├── heap_page_insert_test.cpp
      │   ├── heap_page_persistence_test.cpp
      │   ├── heap_page_reclamation_test.cpp
      │   └── heap_page_validation_test.cpp
      └── tuple/
          ├── tuple_codec_property_test.cpp
          ├── tuple_codec_test.cpp
          ├── tuple_header_test.cpp
          ├── tuple_layout_test.cpp
          └── tuple_varlen_codec_test.cpp

  benchmarks/ remains unchanged.

  ## 3. Production moves performed

  - Page header and raw Page moved to storage/page/.
  - DiskManager moved to storage/disk/.
  - FileSuperblock and PageFile moved to storage/file/.
  - HeapPage, FsmPage, and FsmCandidateIndex moved to storage/heap/.
  - Tuple header, layout, and codec moved to storage/tuple/.
  - common/ now contains only CRC32C, encoding primitives, and identifier types.

  All ordinary production moves are content-identical apart from include directives.

  ## 4–5. HeapPage format/controller extraction

  Created heap_page_format.h/.cpp and moved:

  - All HEAP_PAGE_* and HEAP_SLOT_* persisted constants and offsets.
  - HeapSlotState
  - HeapPageHeader
  - HeapSlotEntry
  - HeapSlotEntryDecodeError
  - HeapSlotEntryDecodeResult
  - EncodeHeapPageHeader
  - DecodeHeapPageHeader
  - EncodeHeapSlotEntry
  - DecodeHeapSlotEntry

  The mutable HeapPage controller, validation, insertion, DEAD transition, compaction, tuple
  access, and mutation logic remain in heap_page.h/.cpp.

  The extracted codec bodies and retained controller bodies compare exactly with the original
  implementation.

  ## 6. types.cpp removal

  src/common/types.cpp was confirmed to contain only an empty namespace translation unit, then
  removed. types.h remains header-only.

  ## 7–8. Test moves and equivalence

  - Ordinary tests moved to matching ownership directories.
  - Identifier, PageId, and Rid tests moved from smoke_test.cpp to common/types_test.cpp.
  - smoke_test.cpp retains only ProjectSmokeTest.TestInfrastructureWorks.
  - Heap tests split into format, validation, insertion, reclamation, and persistence files
    exactly as requested.

  - Tuple test responsibilities remain unchanged.

  An exact body comparison confirmed all 209 original GTest cases appear once with unchanged test
  bodies.

  ## 9. Include paths

  All affected includes now use project-relative paths rooted at src, including:

  #include "storage/disk/disk_manager.h"
  #include "storage/file/file_superblock.h"
  #include "storage/file/page_file.h"
  #include "storage/page/page.h"
  #include "storage/page/page_header.h"
  #include "storage/heap/heap_page.h"
  #include "storage/heap/heap_page_format.h"
  #include "storage/tuple/tuple_codec.h"

  No stale flat-storage include paths or relative ../ includes remain.

  ## 10–13. CMake changes

  - src/CMakeLists.txt:
      - updated all source/header paths;
      - added heap_page_format.cpp/.h;
      - removed types.cpp;
      - moved headers from PUBLIC to the existing PRIVATE source list;
      - retained one dblusblus_core target and the existing public include root.

  - tests/CMakeLists.txt:
      - updated all test paths;
      - added split heap tests and types_test.cpp;
      - retained one test executable and gtest_discover_tests.

  - Root CMakeLists.txt: unchanged.
  - benchmarks/CMakeLists.txt: unchanged.

  ## 14–19. Documentation

  - docs/DEVELOPMENT.md: module-layout guidance now distinguishes the current implemented tree
    from authorization-gated future expansion.

  - VERIFICATION.md: unchanged.
  - AGENTS.md: unchanged.
  - ARCHITECTURE.md: unchanged.
  - docs/PROJECT_STATE.md: advanced to milestone 0029, recording the organization, format/
    controller split, unchanged behavior/formats, and unchanged phase boundary.

  - Added devlog/0029-phase-1-codebase-structural-refactor.md.

  ## 20–28. Verification results

  - Clang Debug discovery: 209 tests
  - Clang Debug: 209/209 passed
  - ASan + UBSan: 209/209 passed
      - LeakSanitizer disabled with ASAN_OPTIONS=detect_leaks=0 because ptrace prevents
        initialization.

  - GCC Debug: 209/209 passed
  - clang-tidy: passed
  - warnings-as-errors: passed
  - clang-format dry-run: passed
  - benchmark target compilation: passed
  - git diff --check: passed

  ## 29. Final Git status summary

  The working tree contains only the expected:

  - production/test path moves and splits;
  - CMake path updates;
  - DEVELOPMENT.md and PROJECT_STATE.md updates;
  - new append-only milestone 0029 devlog.

  Git currently displays moves as deleted old paths plus untracked new paths because nothing was
  staged or committed. No unrelated pre-existing changes were present.

  ## 30–33. Compatibility and observations

  - Persisted formats: unchanged.
  - Production behavior: unchanged.
  - Out-of-scope observations: none requiring follow-up.
  - Architecture questions discovered: none.

  ## 34–36. Phase boundary

  - Phase 1 remains COMPLETE.
  - Implementation Phase 2 remains NOT STARTED.
  - BufferPool remains NOT IMPLEMENTED / NOT IN PROGRESS.
  - HeapFile remains deferred.