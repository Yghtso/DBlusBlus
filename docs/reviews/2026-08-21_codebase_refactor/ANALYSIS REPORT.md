• ## 1. REFACTOR RECOMMENDATION

  Restructuring is worthwhile now.

  The codebase is still small enough for a controlled, behavior-preserving reorganization, while
  the current flat storage/ directory already combines five distinct responsibilities. The
  recommended refactor:

  - keeps common/ limited to cross-subsystem primitives;
  - introduces only directories backed by implemented Phase-1 code;
  - mirrors production ownership in tests;
  - retains central CMake source lists;
  - performs one justified production split and two justified test splits;
  - introduces no BufferPool or other Phase-2 scaffolding.

  This is an organization and dependency-clarity improvement, not a performance or architecture
  change.

  ## 2. CURRENT STRUCTURAL PROBLEMS

  1. common/ contains storage-owned persisted formats.
      - common/page_header.h defines the common persisted page format.
      - common/file_superblock.* defines page-file metadata and validation.
      - Both belong to storage ownership, not general-purpose common utilities.

  2. src/storage/ is too flat.

     Raw POSIX I/O, page-file lifecycle, raw pages, heap/FSM mechanics, and tuple serialization
     are all peers in one directory despite having clear dependency boundaries.

  3. heap_page.h exposes two independently meaningful responsibilities.

     It contains both:
      - persisted heap-header/slot codecs and constants;
      - the mutable HeapPage controller.

     This causes FsmPage, TuplePhysicalLayout, and TupleCodec to include the full HeapPage API
     merely for physical geometry constants.

  4. types.cpp is an empty translation unit.

     It adds build/source-list noise without owning any definitions.

  5. Tests do not reflect component ownership.

     Finding the tests for a source component requires scanning one flat directory.

  6. heap_page_test.cpp is a genuine catch-all.

     It is 1,783 lines and contains 51 tests spanning format codecs, validation, insertion,
     reclamation, compaction, and persistence.

  7. smoke_test.cpp mixes unrelated ownership.

     Only one test is a project smoke test; the remaining six verify identifiers, sentinels,
     PageId, and Rid.

  8. target_sources(... PUBLIC ...) is misleading.

     The headers are not installed/exported and PUBLIC source classification is not what makes
     them includable. Include visibility is already provided by target_include_directories(...
     PUBLIC ...).

  9. DEVELOPMENT.md presents one combined initial layout containing both current and future
     modules.

     That guidance is not wrong, but after this refactor it should distinguish implemented Phase-1
     layout from authorization-dependent future expansion.

  ## 3. CURRENT DEPENDENCY GRAPH

  Current direct production dependencies are:

  common/types
  ├── common/encoding
  │   ├── common/page_header
  │   │   ├── storage/Page
  │   │   ├── storage/DiskManager              [PAGE_SIZE]
  │   │   ├── storage/HeapPage
  │   │   └── storage/FsmPage
  │   ├── common/FileSuperblock
  │   ├── storage/HeapPage codecs
  │   ├── storage/FsmPage codecs
  │   └── storage/TupleHeader + TupleCodec
  └── storage/FsmCandidateIndex

  common/crc32c
  └── common/FileSuperblock

  common/FileSuperblock
  └── storage/PageFile

  storage/DiskManager
  └── storage/PageFile

  storage/Page
  ├── storage/PageFile
  ├── storage/HeapPage
  └── storage/FsmPage

  storage/HeapPage
  ├── storage/FsmPage                          [geometry constants]
  ├── storage/TuplePhysicalLayout              [maximum tuple size]
  └── storage/TupleCodec                       [maximum tuple size]

  storage/TupleHeader
  └── storage/TuplePhysicalLayout
      └── storage/TupleCodec

  storage/FsmPage
  └── storage/FsmCandidateIndex

  Conceptual ownership:

   Component              Current responsibility
  ━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   types                  Cross-subsystem identifiers, sentinels, PageId, Rid
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   encoding               Fixed-width little-endian primitives plus fundamental PageId/Rid codecs
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   crc32c                 Generic checksum primitive
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   page_header            Persisted common page format
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   FileSuperblock         Persisted generic page-file metadata codec
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   Page                   One owned 8 KiB byte buffer plus PageId and common-header access
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   DiskManager            Raw POSIX file registration and positional page I/O
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   PageFile               Managed page-file lifecycle, superblock validation, append allocation
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   HeapPage               Heap format codecs, validation, insertion, DEAD transition, compaction
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   FsmPage                Persisted FSM addressing, category codec, validation, entry access
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   FsmCandidateIndex      Relation-local rebuildable runtime candidate metadata
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   TupleHeader            Persisted physical tuple-header codec
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   TuplePhysicalLayout    Schema-directed physical offsets, null bitmap, size planning
  ─────────────────────  ─────────────────────────────────────────────────────────────────────────
   TupleCodec             Physical scalar/tuple encoding, validation, and borrowed decoding

  The current dependency direction is compatible with the frozen architecture. No architecture/
  code responsibility contradiction was found.

  ## 4. RECOMMENDED TARGET TREE

  ./CMakeLists.txt

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

  benchmarks/
  ├── CMakeLists.txt
  └── benchmark_smoke.cpp

  No empty buffer/, index/, txn/, wal/, catalog/, execution/, or optimizer/ directory is proposed.

  ## 5. COMPLETE OLD -> NEW PATH MAP

  ### Production files

   Current path                           Recommended path                                 Action
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━
   src/main.cpp                           src/main.cpp                                     KEEP
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/crc32c.cpp                  src/common/crc32c.cpp                            KEEP
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/crc32c.h                    src/common/crc32c.h                              KEEP
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/encoding.h                  src/common/encoding.h                            KEEP
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/types.h                     src/common/types.h                               KEEP
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/types.cpp                   src/common/types.h                               MERGE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/page_header.h               src/storage/page/page_header.h                   MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/file_superblock.cpp         src/storage/file/file_superblock.cpp             MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/common/file_superblock.h           src/storage/file/file_superblock.h               MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/disk_manager.cpp           src/storage/disk/disk_manager.cpp                MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/disk_manager.h             src/storage/disk/disk_manager.h                  MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/page.cpp                   src/storage/page/page.cpp                        MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/page.h                     src/storage/page/page.h                          MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/page_file.cpp              src/storage/file/page_file.cpp                   MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/page_file.h                src/storage/file/page_file.h                     MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/heap_page.h                src/storage/heap/heap_page_format.h and src/     SPLIT
                                          storage/heap/heap_page.h
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/heap_page.cpp              src/storage/heap/heap_page_format.cpp and        SPLIT
                                          src/storage/heap/heap_page.cpp
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/fsm_page.cpp               src/storage/heap/fsm_page.cpp                    MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/fsm_page.h                 src/storage/heap/fsm_page.h                      MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/fsm_candidate_index.cpp    src/storage/heap/fsm_candidate_index.cpp         MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/fsm_candidate_index.h      src/storage/heap/fsm_candidate_index.h           MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_header.cpp           src/storage/tuple/tuple_header.cpp               MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_header.h             src/storage/tuple/tuple_header.h                 MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_layout.cpp           src/storage/tuple/tuple_layout.cpp               MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_layout.h             src/storage/tuple/tuple_layout.h                 MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_codec.cpp            src/storage/tuple/tuple_codec.cpp                MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   src/storage/tuple_codec.h              src/storage/tuple/tuple_codec.h                  MOVE

  types.cpp contains only an empty namespace. Removing it does not move or eliminate any
  definition.

  ### Test files

   Current path                           Recommended path                                 Action
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━
   tests/crc32c_test.cpp                  tests/common/crc32c_test.cpp                     MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/encoding_test.cpp                tests/common/encoding_test.cpp                   MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/smoke_test.cpp                   tests/smoke_test.cpp and tests/common/           SPLIT
                                          types_test.cpp
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/disk_manager_test.cpp            tests/storage/disk/disk_manager_test.cpp         MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/file_superblock_test.cpp         tests/storage/file/file_superblock_test.cpp      MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/page_file_test.cpp               tests/storage/file/page_file_test.cpp            MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/page_header_test.cpp             tests/storage/page/page_header_test.cpp          MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/page_test.cpp                    tests/storage/page/page_test.cpp                 MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/fsm_candidate_index_test.cpp     tests/storage/heap/                              MOVE
                                          fsm_candidate_index_test.cpp
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/fsm_page_test.cpp                tests/storage/heap/fsm_page_test.cpp             MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/heap_page_test.cpp               Five tests/storage/heap/heap_page_*_test.cpp     SPLIT
                                          files shown above
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/tuple_header_test.cpp            tests/storage/tuple/tuple_header_test.cpp        MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/tuple_layout_test.cpp            tests/storage/tuple/tuple_layout_test.cpp        MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/tuple_codec_test.cpp             tests/storage/tuple/tuple_codec_test.cpp         MOVE
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/tuple_codec_property_test.cpp    tests/storage/tuple/                             MOVE
                                          tuple_codec_property_test.cpp
  ─────────────────────────────────────  ───────────────────────────────────────────────  ────────
   tests/tuple_varlen_codec_test.cpp      tests/storage/tuple/                             MOVE
                                          tuple_varlen_codec_test.cpp

  ### CMake and benchmark files

   Current path                      Recommended path                  Action
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━
   CMakeLists.txt                    CMakeLists.txt                    KEEP
  ────────────────────────────────  ────────────────────────────────  ────────────────────────
   src/CMakeLists.txt                src/CMakeLists.txt                KEEP, content modified
  ────────────────────────────────  ────────────────────────────────  ────────────────────────
   tests/CMakeLists.txt              tests/CMakeLists.txt              KEEP, content modified
  ────────────────────────────────  ────────────────────────────────  ────────────────────────
   benchmarks/CMakeLists.txt         benchmarks/CMakeLists.txt         KEEP
  ────────────────────────────────  ────────────────────────────────  ────────────────────────
   benchmarks/benchmark_smoke.cpp    benchmarks/benchmark_smoke.cpp    KEEP

  ## 6. FILE SPLIT/RENAME DECISIONS

  ### heap_page.h/.cpp: SPLIT

  Move persisted-format declarations and implementations into heap_page_format.*:

  - heap format constants and offsets;
  - HeapSlotState;
  - HeapPageHeader;
  - HeapSlotEntry;
  - header and slot codec errors/results;
  - Encode/DecodeHeapPageHeader;
  - Encode/DecodeHeapSlotEntry.

  Keep in heap_page.*:

  - page validation results;
  - insertion, DEAD-transition, and compaction results;
  - HeapPage controller;
  - page initialization, validation, insertion, lookup, DEAD transition, and compaction.

  Reason: this is an existing responsibility boundary at approximately line 171 of the
  implementation. It also lets FSM and tuple layout depend only on physical heap geometry rather
  than the complete mutable page controller.

  ### heap_page_test.cpp: SPLIT

  Exact assignment:

  - heap_page_format_test.cpp
      - every HeapPageHeaderCodecTest;
      - every HeapSlotEntryCodecTest.

  - heap_page_validation_test.cpp
      - every HeapPageTest;
      - every HeapPageValidationTest.

  - heap_page_insert_test.cpp
      - every HeapPageInsertionTest.

  - heap_page_reclamation_test.cpp
      - every non-integration HeapPageDeadTransitionTest;
      - every non-integration HeapPageCompactionTest.

  - heap_page_persistence_test.cpp
      - HeapPageDeadTransitionIntegrationTest.PersistsDeadStateWithoutRemovingTupleBytes;
      - HeapPageCompactionIntegrationTest.PersistsCompactedGeometryStatesAndPayloads;
      - HeapPageIntegrationTest.PersistsAllocatedBlankHeapPageThroughDiskManager.

  No test names or assertions should change.

  ### smoke_test.cpp: SPLIT

  Keep only:

  - ProjectSmokeTest.TestInfrastructureWorks

  in tests/smoke_test.cpp.

  Move these suites to tests/common/types_test.cpp:

  - IdentifierTypesTest;
  - PageIdTest;
  - RidTest;
  - their associated type/sentinel static assertions.

  ### tuple_codec.cpp: KEEP

  At 694 lines it is substantial but remains one cohesive physical tuple codec:

  - scalar encoding;
  - VARCHAR descriptor encoding;
  - tuple construction;
  - tuple validation;
  - per-column decoding.

  No independently consumed scalar-codec module exists yet. Splitting it now would add API and
  file churn without improving current dependency direction.

  ### Tuple tests: KEEP

  The existing division is already meaningful:

  - fixed scalar/fixed tuple;
  - varlen tuple;
  - randomized property;
  - tuple header;
  - tuple layout.

  The 708- and 755-line codec tests are long but cohesive and already separated by responsibility.

  ### Other renames

  No pure filename rename is recommended. Existing component names remain accurate and durable.

  Moved header guards should be updated to match their new paths. No namespace or API rename is
  needed.

  ## 7. INCLUDE-PATH PLAN

  Use one convention everywhere:

  #include "common/encoding.h"
  #include "storage/file/page_file.h"
  #include "storage/heap/heap_page.h"
  #include "storage/tuple/tuple_codec.h"

  Rules:

  - project-relative from ${PROJECT_SOURCE_DIR}/src;
  - no ../ or ../../;
  - every .cpp includes its own header through the full project-relative path;
  - tests use the same paths;
  - retain the current public source include root for clangd and test targets.

  ### Header replacements

   Old include                      New include
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   common/page_header.h             storage/page/page_header.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   common/file_superblock.h         storage/file/file_superblock.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/disk_manager.h           storage/disk/disk_manager.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/page.h                   storage/page/page.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/page_file.h              storage/file/page_file.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/heap_page.h              storage/heap/heap_page.h or storage/heap/heap_page_format.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/fsm_page.h               storage/heap/fsm_page.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/fsm_candidate_index.h    storage/heap/fsm_candidate_index.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/tuple_header.h           storage/tuple/tuple_header.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/tuple_layout.h           storage/tuple/tuple_layout.h
  ───────────────────────────────  ─────────────────────────────────────────────────────────────
   storage/tuple_codec.h            storage/tuple/tuple_codec.h

  ### Consumers requiring updates

  - page_header.h: FileSuperblock, DiskManager, Page, HeapPage, FsmPage, and their page/file/heap
    tests.

  - file_superblock.h: its implementation, PageFile, file tests, FSM/heap persistence tests, and
    varlen persistence tests.

  - disk_manager.h: its implementation, PageFile, and all disk-backed page/file/heap/FSM/tuple
    tests.

  - page.h: its implementation, PageFile implementation, HeapPage, FsmPage, and tests using owned
    raw pages.

  - page_file.h: its implementation and persistence/integration tests.
  - fsm_page.h: its implementation, FsmCandidateIndex implementation, and both FSM test files.
  - fsm_candidate_index.h: its implementation and test.
  - tuple_header.h: its implementation, TuplePhysicalLayout, and tuple-header/layout tests.
  - tuple_layout.h: its implementation, TupleCodec, and tuple layout/codec tests.
  - tuple_codec.h: its implementation and three codec tests.

  After the HeapPage split:

  - FsmPage, TuplePhysicalLayout, and TupleCodec should include heap_page_format.h for geometry
    constants.

  - Controller users should include heap_page.h.
  - heap_page.h itself should include heap_page_format.h.

  No change is needed for common/types.h, common/encoding.h, or common/crc32c.h.

  ## 8. CMAKE PLAN

  ### ./CMakeLists.txt: KEEP

  No responsibility change is needed. It correctly owns project configuration, shared options,
  testing enablement, and subdirectory delegation.

  ### ./src/CMakeLists.txt: MODIFY

  Update source paths for the new hierarchy, add:

  storage/heap/heap_page_format.cpp
  storage/heap/heap_page_format.h

  and remove the empty:

  common/types.cpp

  Keep one central source list. The current codebase is too small to justify five additional
  subdirectory CMake files.

  Change headers in target_sources from PUBLIC to PRIVATE. This preserves IDE/source-list
  visibility without publishing headers as interface sources.

  Retain:

  target_include_directories(
      dblusblus_core
      PUBLIC
          "${PROJECT_SOURCE_DIR}/src"
  )

  Do not add install/export logic or FILE_SET HEADERS; neither has current value.

  ### ./tests/CMakeLists.txt: MODIFY

  Replace flat test paths with the target-tree paths and add the split heap/types files.

  Retain:

  - one dblusblus_tests executable;
  - GTest::gtest_main;
  - gtest_discover_tests.

  Do not create per-test-module CMake files or separate test executables.

  ### ./benchmarks/CMakeLists.txt: KEEP

  The benchmark suite has one smoke source and no ownership problem. No benchmark scaffolding is
  justified.

  ### Additional CMake files

  None.

  CMakePresets.json also requires no change because target names and configuration behavior remain
  unchanged.

  ## 9. DEVELOPMENT.md PLAN

  UPDATE only the Initial Module Layout section.

  Recommended narrow edit:

  1. Rename it to distinguish current layout from future expansion.
  2. Add the actual implemented Phase-1 tree from this review.
  3. Retain the existing deeper future layout as explicitly authorization-dependent expansion.
  4. State that future directories are created only when their corresponding subsystem is
     implemented.

  Do not change:

  - phase sequencing;
  - Phase-2 contents;
  - BufferPool-first boundary;
  - milestone semantics;
  - architecture contracts.

  ## 10. VERIFICATION.md PLAN

  NO CHANGE.

  The document defines test obligations and procedures, not concrete source/test paths. Mirroring
  tests under subsystem directories does not alter any verification requirement.

  ## 11. AGENTS.md PLAN

  NO CHANGE.

  Its existing instructions already cover:

  - architecture authority;
  - current-state workflow;
  - no speculative abstractions;
  - target-oriented CMake;
  - testing discipline;
  - phase-gate enforcement.

  Embedding the current tree in AGENTS.md would create unnecessary maintenance duplication.

  ## 12. ARCHITECTURE.md PLAN

  NO CHANGE.

  The architecture explicitly states that concrete source directories and filenames are not
  architectural requirements. The proposed organization follows its existing subsystem boundaries
  without changing:

  - responsibilities;
  - persisted layouts;
  - validation;
  - ownership;
  - errors;
  - lifetimes;
  - SQL or runtime behavior.

  No architecture/code responsibility contradiction was discovered.

  ## 13. PROJECT_STATE / DEVLOG PLAN

  After the later refactor succeeds:

  ### docs/PROJECT_STATE.md

  UPDATE with a concise current-state note that:

  - Phase-1 source/tests now use the implemented common, disk, file, page, heap, and tuple
    ownership layout;

  - behavior and persisted formats remain unchanged;
  - the verified test population remains 209;
  - Phase 1 remains complete;
  - Phase 2 remains not started;
  - BufferPool remains unimplemented and not in progress.

  Add the completed milestone to the milestone index without turning the document into a migration
  narrative.

  ### devlog/

  Append the next available numbered entry at implementation time, conceptually titled:

  Phase-1 codebase structural refactor

  Record moved/split files, unchanged behavior, build/test verification, and the unchanged Phase-2
  boundary.

  Do not call it a Phase-2 milestone.

  ## 14. SAFE IMPLEMENTATION SEQUENCE

  1. Record the clean baseline.
      - git status --short
      - ctest --preset clang-debug -N
      - confirm 209 discovered tests.

  2. Establish only the target directories.
      - storage/disk
      - storage/file
      - storage/page
      - storage/heap
      - storage/tuple
      - matching test directories.

  3. Move the page module.
      - move page_header.h;
      - move page.*;
      - update includes and guards;
      - update src/CMakeLists.txt;
      - build dblusblus_core;
      - run page-header/page tests.

  4. Move raw I/O and page-file modules.
      - move DiskManager;
      - move FileSuperblock and PageFile;
      - update includes/CMake;
      - build;
      - run disk, file-superblock, PageFile, and Page tests.

  5. Split and move HeapPage.
      - extract format declarations/definitions without changing code;
      - move controller code;
      - update FSM/tuple dependencies to use heap_page_format.h;
      - build;
      - run the unchanged heap test before reorganizing that test.

  6. Move FSM files into storage/heap.
      - update includes/CMake;
      - build;
      - run both FSM test suites.

  7. Move tuple files into storage/tuple.
      - update includes/CMake;
      - build;
      - run all tuple tests.

  8. Remove the empty types.cpp.
      - remove it from CMake;
      - build core and smoke tests.

  9. Reorganize ordinary tests by subsystem.

     Move files without changing test bodies.

  10. Split smoke_test.cpp and heap_page_test.cpp.

     Preserve all existing TEST suite and case names.

  11. Update tests/CMakeLists.txt.

     Reconfigure so gtest_discover_tests regenerates discovery metadata.

  12. Run the complete verification matrix.
  13. Inspect the complete diff.

     Confirm moves remain content-identical except required includes, guards, CMake paths, and
     approved splits.

  14. Update DEVELOPMENT.md, PROJECT_STATE.md, and append the devlog only after verification
     succeeds.

  Use ordinary file moves such as git mv; avoid delete-and-recreate rewrites. Do not perform
  unrelated formatting.

  ## 15. POST-REFACTOR VERIFICATION

  Recommended actual repository commands:

  cmake --preset clang-debug
  cmake --build --preset clang-debug
  ctest --preset clang-debug -N
  ctest --preset clang-debug

  Expected discovery/result baseline:

  209 tests discovered
  209 / 209 passed

  Sanitizers:

  cmake --preset clang-asan
  cmake --build --preset clang-asan
  ctest --preset clang-asan

  In the current ptrace environment, use the established leak-sanitizer accommodation if still
  required:

  ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan

  GCC:

  cmake --preset gcc-debug
  cmake --build --preset gcc-debug
  ctest --preset gcc-debug

  Clang-tidy:

  cmake --preset clang-tidy
  cmake --build --preset clang-tidy

  Warnings as errors:

  cmake --preset clang-debug -DDBLUSBLUS_WARNINGS_AS_ERRORS=ON
  cmake --build --preset clang-debug

  Formatting check without mutation:

  clang-format --dry-run --Werror $(rg --files src tests benchmarks -g '*.cpp' -g '*.h')

  Benchmark target compilation, without making a performance claim:

  cmake --preset clang-bench
  cmake --build --preset clang-bench

  Final hygiene:

  git diff --check
  git status --short

  The required equivalence is identical test names, test count, persisted bytes, errors, and
  runtime behavior. Test file splitting must not change GTest case organization.

  ## 16. OUT-OF-SCOPE IMPROVEMENTS DISCOVERED

  These should not be mixed into the structural refactor:

  - redesigning DiskStatus, DiskResult, PageFileResult, or other error APIs;
  - changing DiskManager lifecycle synchronization for future BufferPool concurrency;
  - introducing BufferPool-aware page ownership;
  - adding relation-wide FSM ownership or renaming FsmCandidateIndex;
  - redesigning tuple values or replacing the current construction-time std::variant;
  - changing tuple size limits, codecs, validation, or layout;
  - changing ordinary-page checksum/WAL behavior;
  - extracting a shared temporary-directory test framework;
  - splitting TupleCodec before an independent consumer creates a real boundary;
  - performance tuning DiskManager, FSM buckets, tuple encoding, or compaction.

  All are explicitly excluded from the eventual refactor.

  ## 17. RISKS

  1. Include-path breakage.

     The main risk is missing transitive consumers, especially integration tests that include
     disk/file/page headers.

  2. Incorrect HeapPage split.

     Constants or codec helpers could accidentally change linkage, visibility, or validation
     behavior. Extraction should be mechanical.

  3. Test loss during splitting.

     heap_page_test.cpp contains 51 cases. Discovery must remain exactly 209 total tests.

  4. CMake stale paths.

     A source may remain in an old build directory even after CMake lists are wrong.
     Reconfiguration and a clean discovery count are essential.

  5. Header-guard collisions.

     Moved headers should receive path-appropriate unique guards.

  6. Accidental semantic edits.

     Moving format codecs creates temptation to rename types or alter validation. That must be
     rejected.

  7. Unrelated formatting churn.

     Pure moves should remain byte-identical where possible.

  8. Premature Phase-2 scaffolding.

     No storage/buffer/, PageGuard interface, HeapFile placeholder, or future directory should be
     created.

  9. Documentation overreach.

     DEVELOPMENT.md should describe layout only; ARCHITECTURE.md must remain untouched.

  10. Git history obscurity.

     Use normal moves and avoid delete/recreate operations. Git rename detection remains heuristic
     and is not itself a correctness requirement.

  ## 18. FINAL BOUNDARY CONFIRMATION

  - Files modified: NONE
  - Production behavior changed: NO
  - Architecture changed: NO
  - git diff --check: clean
  - Final git status --short: clean
  - Phase 1 remains: COMPLETE
  - Implementation Phase 2 remains: NOT STARTED
  - BufferPool remains: NOT IMPLEMENTED / NOT IN PROGRESS
  - HeapFile remains deferred behind BufferPool.
  - No Phase-2 code, directory, placeholder, task, or implementation prompt was created.