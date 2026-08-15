# Architecture Rewrite — Final Coverage Ledger

Legacy source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`

Every legacy numbered section `0..725` has exactly one final disposition.

| Old | Title | Pass | Historical migration status | Final destination |
|---:|---|---:|---|---|
| 0 | How Codex should use this file | 1 | MOVED_OUT_PASS1 | AGENTS.md |
| 1 | Project Goal | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 2 | Non-Goals | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 3 | Architectural Principles | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 4 | Platform | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 5 | Implementation Language | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 6 | Persistent Storage Model | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 7 | File Organization | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 8 | Disk / I/O Layer | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 9 | On-Disk Serialization | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 10 | Table Storage | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 11 | Heap Page Layout | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 12 | Tuple Representation | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 13 | Free-Space Management | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 14 | Buffer Pool | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 15 | Buffer Replacement | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 16 | Indexing | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 17 | B+ Tree Concurrency | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 18 | MVCC | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 19 | Transaction Model | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 20 | Locks | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 21 | Write-Ahead Log | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 22 | Commit Path | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 23 | Buffer/WAL Policy | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 24 | Crash Recovery | 1 | MIGRATED_REFINED_PASS1 | ARCHITECTURE.md |
| 25 | Checkpointing | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 26 | Vacuum / Garbage Collection | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 27 | Catalog | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 28 | SQL Front End | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 29 | Binder | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 30 | Logical Query Representation | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 31 | Query Rewrite | 1 | MIGRATED_SPLIT_PASS1 | ARCHITECTURE.md |
| 32 | Statistics | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 33 | Cost-Based Optimizer | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 34 | Join Ordering | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 35 | Physical Operators | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 36 | Execution Model | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 37 | Execution Data Layout | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 38 | Primary Join Algorithms | 1 | MIGRATED_SPLIT_PASS1 | ARCHITECTURE.md |
| 39 | Aggregation | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 40 | Sorting | 1 | MIGRATED_SPLIT_PASS1 | ARCHITECTURE.md |
| 41 | Query Memory Management | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 42 | Parallelism | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 43 | Threading / Shared State | 1 | MIGRATED_BASELINE_PASS1 | ARCHITECTURE.md |
| 44 | Error Handling | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 45 | Testing Philosophy | 1 | MIGRATED_ARCH_REQUIREMENTS_PASS1 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 46 | Benchmarking Philosophy | 1 | MIGRATED_ARCH_REQUIREMENTS_PASS1 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 47 | Observability | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 48 | Explain | 1 | MIGRATED_PASS1 | ARCHITECTURE.md |
| 49 | Suggested Implementation Order | 1 | EXCLUDED_FROM_ARCHITECTURE_PASS1 | DEVELOPMENT.md |
| 50 | Architecture Invariants | 1 | MIGRATED_INVARIANTS_PASS1 | ARCHITECTURE.md |
| 51 | Locked Architecture Decisions | 1 | MIGRATED_DEDUP_PASS1 | ARCHITECTURE.md |
| 52 | Working Philosophy | 1 | MIGRATED_SPLIT_PASS1 | ARCHITECTURE.md |
| 53 | Concrete Storage Engine Contract | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 54 | Fundamental Identifier Types | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 55 | PageId | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 56 | RID | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 57 | Index-to-Heap Addressing | 2 | MIGRATED_CROSS_SUBSYSTEM_PASS2 | ARCHITECTURE.md |
| 58 | File Kinds | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 59 | File Superblock | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 60 | Page Allocation Policy | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 61 | Common Page Header | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 62 | Page Types | 2 | MIGRATED_PASS2 | ARCHITECTURE.md |
| 63 | Page Checksums | 2 | MIGRATED_WITH_ISSUE_PASS2 | ARCHITECTURE.md |
| 64 | Heap File Structure | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 65 | Heap Scan Order | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 66 | Heap Data Page Header | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 67 | Slot Directory | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 68 | Heap Page Free-Space Geometry | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 69 | Maximum Inline Tuple Size | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 70 | Tuple Header | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 71 | Tuple Flags | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 72 | Tuple Data Layout | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 73 | Null Bitmap | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 74 | Fixed-Width Values | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 75 | VARCHAR Representation | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 76 | Tuple Alignment | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 77 | Schema Versioning | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 78 | INSERT Path | 3 | MIGRATED_WITH_BOUNDARY_PASS3 | ARCHITECTURE.md |
| 79 | UPDATE Path | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 80 | DELETE Path | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 81 | MVCC Visibility Boundary | 3 | MIGRATED_PASS3 | ARCHITECTURE.md |
| 82 | Free-Space Map | 4 | MIGRATED_PASS4 | ARCHITECTURE.md |
| 83 | FSM Persistence Semantics | 4 | MIGRATED_PASS4 | ARCHITECTURE.md |
| 84 | Heap Page Compaction | 4 | MIGRATED_PASS4 | ARCHITECTURE.md |
| 85 | Vacuum Physical Reclamation | 4 | MIGRATED_PASS4 | ARCHITECTURE.md |
| 86 | DiskManager Responsibilities | 5 | MIGRATED_WITH_REFINEMENT_PASS5 | ARCHITECTURE.md |
| 87 | File I/O Semantics | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 88 | BufferPool Responsibilities | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 89 | Buffer Frame | 5 | MIGRATED_BASELINE_PASS5 | ARCHITECTURE.md |
| 90 | Page Guards | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 91 | Buffer-Pool Page Table | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 92 | Pinning Semantics | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 93 | Dirty-Page Semantics | 5 | MIGRATED_WITH_LATER_REFINEMENT_PASS5 | ARCHITECTURE.md |
| 94 | WAL-before-Data Enforcement Point | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 95 | CLOCK Replacement Contract | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 96 | Storage Object Boundaries | 5 | MIGRATED_CROSS_CHAPTER_PASS5 | ARCHITECTURE.md |
| 97 | HeapPage API Shape | 5 | MIGRATED_CROSS_CHAPTER_PASS5 | ARCHITECTURE.md |
| 98 | TupleCodec Boundary | 5 | MIGRATED_CROSS_CHAPTER_PASS5 | ARCHITECTURE.md |
| 99 | Zero-Copy vs Decode Policy | 5 | MIGRATED_CROSS_CHAPTER_PASS5 | ARCHITECTURE.md |
| 100 | Lifetime Safety Rule | 5 | MIGRATED_PASS5 | ARCHITECTURE.md |
| 101 | Initial Module Layout | 5 | MOVED_OUT_SPLIT_PASS5 | DEVELOPMENT.md |
| 102 | Storage-Layer Dependency Direction | 5 | MIGRATED_CROSS_CHAPTER_PASS5 | ARCHITECTURE.md |
| 103 | Storage Milestone 1 | 5 | MOVED_OUT_PASS5 | DEVELOPMENT.md |
| 104 | Storage Milestone 1 Required Tests | 5 | SPLIT_VERIFICATION_PASS5 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 105 | Storage Milestone 1 Benchmarks | 5 | SPLIT_PERFORMANCE_PASS5 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 106 | Deliberately Deferred Storage Features | 5 | CONSOLIDATED_DEFERRED_PASS5 | ARCHITECTURE.md |
| 107 | Storage Decisions Record | 5 | MERGED_DEDUP_PASS5 | ARCHITECTURE.md |
| 108 | Next Architecture Topic | 5 | MOVED_OUT_PASS5 | DEVELOPMENT.md |
| 109 | B+ Tree Architecture Contract | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 110 | B+ Tree File and Superblock | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 111 | Index Key Schema | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 112 | User Key vs Physical Key | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 113 | Persistent RID Encoding in Indexes | 6 | MIGRATED_STRICT_PASS6 | ARCHITECTURE.md |
| 114 | Order-Preserving Index Key Encoding | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 115 | Index Field Encoding | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 116 | Composite Keys | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 117 | Maximum Encoded User Key | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 118 | Physical-Key Comparison and Search Sentinels | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 119 | Node Page Organization | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 120 | B+ Tree Slot Entry | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 121 | Leaf Header | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 122 | Leaf Entry | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 123 | Internal Header | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 124 | Internal Entry | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 125 | Internal Representation and Separator Semantics | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 126 | When a Separator Must Change | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 127 | Internal Search | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 128 | Leaf Search | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 129 | Split Trigger and Compaction | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 130 | Leaf Split | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 131 | Leaf Sibling Links on Split | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 132 | Internal Split | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 133 | Root Split and Contraction | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 134 | Occupancy and Underflow | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 135 | Redistribution and Merge Policy | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 136 | Leaf Merge | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 137 | Internal Rebalancing | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 138 | Tree-Local Free Page List | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 139 | Safe Page Reuse | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 140 | Point Lookup Concurrency | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 141 | Write Concurrency | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 142 | Root Metadata Synchronization | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 143 | Latch Ordering | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 144 | Forward Range Scan | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 145 | Leaf Handoff During Range Scan | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 146 | Reverse Scans | 6 | CONSOLIDATED_DEFERRED_PASS6 | ARCHITECTURE.md |
| 147 | B+ Tree API Boundary | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 148 | IndexKeyCodec Boundary | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 149 | Duplicate SQL Keys | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 150 | Unique Index Enforcement | 6 | MIGRATED_BOUNDARY_PASS6 | ARCHITECTURE.md |
| 151 | UNIQUE and NULL | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 152 | MVCC Interaction | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 153 | Index-Only Scans | 6 | CONSOLIDATED_DEFERRED_PASS6 | ARCHITECTURE.md |
| 154 | UPDATE and DELETE Interaction | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 155 | Vacuum Index Cleanup | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 156 | HOT-Like Optimization | 6 | CONSOLIDATED_DEFERRED_PASS6 | ARCHITECTURE.md |
| 157 | Index Scan Cost Awareness | 6 | MIGRATED_CROSS_CHAPTER_PASS6 | ARCHITECTURE.md |
| 158 | Prefix Compression | 6 | CONSOLIDATED_DEFERRED_PASS6 | ARCHITECTURE.md |
| 159 | Internal and Leaf Search Performance | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 160 | Root/Internal Page Caching | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 161 | Structural Modification vs User Transaction | 6 | MIGRATED_WITH_LATER_REFINEMENT_PASS6 | ARCHITECTURE.md |
| 162 | WAL Direction for Structural Operations | 6 | MIGRATED_WITH_LATER_REFINEMENT_PASS6 | ARCHITECTURE.md |
| 163 | Page LSN and WAL Ordering | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 164 | Structural Publication Invariant | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 165 | Page Validation | 6 | MIGRATED_WITH_FORMAT_GAP_PASS6 | ARCHITECTURE.md |
| 166 | Full Tree Verifier | 6 | MIGRATED_PASS6 | ARCHITECTURE.md |
| 167 | B+ Tree Milestone 1 | 6 | MOVED_OUT_PASS6 | DEVELOPMENT.md |
| 168 | B+ Tree Milestone 2 | 6 | MOVED_OUT_PASS6 | DEVELOPMENT.md |
| 169 | B+ Tree Milestone 3 | 6 | MOVED_OUT_PASS6 | DEVELOPMENT.md |
| 170 | Required Deterministic Tests | 6 | SPLIT_VERIFICATION_PASS6 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 171 | Duplicate Stress Test | 6 | SPLIT_VERIFICATION_PASS6 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 172 | Randomized Tests | 6 | SPLIT_VERIFICATION_PASS6 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 173 | Concurrent Tests | 6 | SPLIT_VERIFICATION_PASS6 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 174 | B+ Tree Benchmarks | 6 | SPLIT_PERFORMANCE_PASS6 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 175 | Deliberately Deferred B+ Tree Features | 6 | CONSOLIDATED_DEFERRED_PASS6 | ARCHITECTURE.md |
| 176 | First Post-Correctness Optimization Candidates | 6 | CONSOLIDATED_FUTURE_PASS6 | ARCHITECTURE.md |
| 177 | B+ Tree Invariants | 6 | MIGRATED_INVARIANTS_PASS6 | ARCHITECTURE.md |
| 178 | Decisions Added by the B+ Tree Architecture | 6 | MERGED_DEDUP_PASS6 | ARCHITECTURE.md |
| 179 | Next Architecture Stage | 6 | MOVED_OUT_PASS6 | DEVELOPMENT.md |
| 180 | Transaction, MVCC, Locking, WAL, Recovery, and Vacuum Contract | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 181 | Architecture Refinement: No Physical User-Transaction Undo | 7 | MIGRATED_REFINEMENT_PASS7 | ARCHITECTURE.md |
| 182 | Reserved Transaction IDs | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 183 | Durable Transaction-ID Reservation | 7 | MIGRATED_WITH_GAP_PASS7 | ARCHITECTURE.md |
| 184 | Transaction Object | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 185 | Isolation Levels | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 186 | Command IDs | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 187 | Snapshot Representation | 7 | MIGRATED_WITH_GAP_PASS7 | ARCHITECTURE.md |
| 188 | Snapshot Capture Synchronization | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 189 | READ COMMITTED Snapshots | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 190 | REPEATABLE READ Snapshots | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 191 | Transaction Status Store | 7 | MIGRATED_WITH_FORMAT_GAP_PASS7 | ARCHITECTURE.md |
| 192 | Transaction Status Page Mapping | 7 | MIGRATED_WITH_FORMAT_GAP_PASS7 | ARCHITECTURE.md |
| 193 | Transaction Status Lookup | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 194 | Commit Status Publication | 7 | MIGRATED_BOUNDARY_PASS7 | ARCHITECTURE.md |
| 195 | Abort Status Publication | 7 | MIGRATED_BOUNDARY_PASS7 | ARCHITECTURE.md |
| 196 | Read-Only Transactions | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 197 | Tuple Visibility: Creator Rule | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 198 | Tuple Visibility: Deleter Rule | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 199 | Visibility Algorithm Summary | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 200 | Hint Cleanup | 7 | MIGRATED_BOUNDARY_PASS7 | ARCHITECTURE.md |
| 201 | Logical Lock Types | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 202 | Tuple Write Lock Key | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 203 | Critical Lock/Latch Rule | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 204 | UPDATE / DELETE Write Conflict Protocol | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 205 | READ COMMITTED Write Conflict Behavior | 7 | MIGRATED_BOUNDARY_PASS7 | ARCHITECTURE.md |
| 206 | REPEATABLE READ Write Conflict Behavior | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 207 | Lost-Update Prevention | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 208 | Unique-Key Lock Key | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 209 | Unique-Key Lock Protocol | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 210 | Unique Check Semantics | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 211 | Lock Duration | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 212 | Lock Table | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 213 | Deadlock Detection | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 214 | B+ Tree Latches Are Not LockManager Locks | 7 | MIGRATED_PASS7 | ARCHITECTURE.md |
| 215 | WAL Physical Organization | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 216 | WAL Record Alignment | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 217 | WAL Record Header | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 218 | Per-Transaction WAL Chain | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 219 | WAL Record Types | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 220 | PAGE_DELTA Record | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 221 | PAGE_INIT Record | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 222 | Torn-Page Protection | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 223 | Full-Page Image Semantics | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 224 | Why Full-Page Images Are Checkpoint-Epoch Based | 8 | MIGRATED_RATIONALE_PASS8 | ARCHITECTURE.md |
| 225 | B+ Tree Mini-Transactions | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 226 | Atomic BTREE_MTR WAL Record | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 227 | B+ Mini-Transaction No-Flush Barrier | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 228 | B+ Tree Changes Survive User Abort | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 229 | Heap WAL Ordering Relative to Index WAL | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 230 | WAL Buffer | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 231 | WAL Writer / Flusher | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 232 | Group Commit | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 233 | Commit Coordinator | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 234 | Synchronous Commit | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 235 | Dirty Page recLSN | 8 | MIGRATED_CROSS_CHAPTER_PASS8 | ARCHITECTURE.md |
| 236 | WAL-Before-Data Restatement | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 237 | Database Control File | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 238 | Control File Update Frequency | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 239 | Fuzzy Checkpoint Goal | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 240 | Checkpoint Protocol | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 241 | Dirty-Page Table Checkpoint Entry | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 242 | Active Writer Transaction Checkpoint Entry | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 243 | Checkpoint Data Chunking | 8 | MIGRATED_WITH_FORMAT_GAP_PASS8 | ARCHITECTURE.md |
| 244 | Checkpoint Redo Start | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 245 | WAL Recycling | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 246 | Recovery Startup: WAL Tail Validation | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 247 | Recovery Phase 1: Analysis | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 248 | Recovery Phase 2: Redo | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 249 | Redo and Corrupt/Torn Data Pages | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 250 | Recovery Phase 3: Loser Resolution | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 251 | No CLRs in v1 User-DML Recovery | 8 | MIGRATED_REFINEMENT_PASS8 | ARCHITECTURE.md |
| 252 | Recovery of Transaction Status Pages | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 253 | Recovery of Approximate Metadata | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 254 | Recovery Completion | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 255 | Crash-Recovery Correctness Principle | 8 | MIGRATED_PASS8 | ARCHITECTURE.md |
| 256 | Vacuum Global Visibility Horizon | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 257 | Why Active Transactions Alone Are Not the Vacuum Horizon | 9 | MIGRATED_RATIONALE_PASS9 | ARCHITECTURE.md |
| 258 | Tuple Version Garbage Eligibility | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 259 | Vacuum Treatment of In-Progress Metadata | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 260 | Two-Phase Physical RID Reclamation | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 261 | Read Epoch Manager | 9 | MIGRATED_WITH_GAP_PASS9 | ARCHITECTURE.md |
| 262 | Why Snapshot Visibility Alone Is Not Enough for RID Reuse | 9 | MIGRATED_RATIONALE_PASS9 | ARCHITECTURE.md |
| 263 | DEAD Slot Persistence Across Crash | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 264 | Vacuum Index-Cleanup Protocol | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 265 | Vacuum Version-Chain Splicing | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 266 | Vacuum and Concurrent Updates | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 267 | DEAD to UNUSED Transition | 9 | MIGRATED_WITH_GAP_PASS9 | ARCHITECTURE.md |
| 268 | Vacuum Handling of Aborted `xmax` | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 269 | Freezing Ancient Live Tuples | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 270 | Transaction Status Truncation | 9 | MIGRATED_WITH_COHERENCE_GAP_PASS9 | ARCHITECTURE.md |
| 271 | Vacuum and B+ Tree Garbage | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 272 | Vacuum and FSM | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 273 | Vacuum and Statistics | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 274 | Manual Vacuum First | 9 | MIGRATED_WITH_CLASSIFICATION_PASS9 | ARCHITECTURE.md |
| 275 | Transaction + Index + Heap INSERT Protocol | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 276 | Transaction + Index + Heap UPDATE Protocol | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 277 | Transaction + Heap DELETE Protocol | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 278 | Transaction COMMIT Protocol | 9 | MIGRATED_INTEGRATION_PASS9 | ARCHITECTURE.md |
| 279 | Transaction ABORT Protocol | 9 | MIGRATED_INTEGRATION_PASS9 | ARCHITECTURE.md |
| 280 | Statement Retry Boundary | 9 | MIGRATED_PASS9 | ARCHITECTURE.md |
| 281 | Error Categories | 9 | MIGRATED_CROSSCUTTING_PASS9 | ARCHITECTURE.md |
| 282 | Transaction Observability | 9 | MIGRATED_CROSSCUTTING_PASS9 | ARCHITECTURE.md |
| 283 | Transaction Debug Introspection | 9 | MIGRATED_CROSSCUTTING_PASS9 | ARCHITECTURE.md |
| 284 | Crash Injection Framework | 9 | MIGRATED_VERIFICATION_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 285 | Recovery Property Tests | 9 | MIGRATED_VERIFICATION_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 286 | MVCC Visibility Tests | 9 | MIGRATED_VERIFICATION_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 287 | Isolation Tests | 9 | MIGRATED_VERIFICATION_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 288 | Locking Tests | 9 | MIGRATED_VERIFICATION_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 289 | Group Commit Benchmarks | 9 | MIGRATED_PERFORMANCE_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 290 | Checkpoint/Recovery Benchmarks | 9 | MIGRATED_PERFORMANCE_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 291 | Vacuum Benchmarks | 9 | MIGRATED_PERFORMANCE_PASS9 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 292 | Deliberately Deferred Transaction Features | 9 | MIGRATED_DEFERRED_PASS9 | ARCHITECTURE.md |
| 293 | High-Reward Future Transaction Experiments | 9 | MIGRATED_DEFERRED_PASS9 | ARCHITECTURE.md |
| 294 | Transaction/Durability Invariants | 9 | RECONCILED_PASS9 | ARCHITECTURE.md |
| 295 | Modules Added by This Architecture | 9 | CLASSIFIED_PASS9 | DEVELOPMENT.md |
| 296 | Implementation Order for the Transaction/Durability Core | 9 | CLASSIFIED_PASS9 | DEVELOPMENT.md |
| 297 | Transaction/Durability Milestone 1 | 9 | CLASSIFIED_PASS9 | DEVELOPMENT.md |
| 298 | Transaction/Durability Milestone 2 | 9 | CLASSIFIED_PASS9 | DEVELOPMENT.md |
| 299 | Transaction/Durability Milestone 3 | 9 | CLASSIFIED_PASS9 | DEVELOPMENT.md |
| 300 | Architecture Status After This Section | 9 | MIGRATED_WITH_CLASSIFICATION_PASS9 | ARCHITECTURE.md + PROJECT_STATE.md/devlog |
| 301 | Catalog, SQL Front End, Binder, Expression, and Logical Plan Contract | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 302 | Upper-Layer Dependency Direction | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 303 | Catalog Philosophy | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 304 | Database Namespace Model | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 305 | Catalog Object Identifiers | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 306 | Catalog System Tables | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 307 | Catalog Bootstrap | 10 | MIGRATED_WITH_GAP_PASS10 | ARCHITECTURE.md |
| 308 | Catalog Cache | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 309 | Catalog Descriptor Lifetime | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 310 | Schema Versioning | 10 | MIGRATED_INTEGRATION_PASS10 | ARCHITECTURE.md |
| 311 | Column Identity vs Position | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 312 | SQL Type System | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 313 | LogicalType Representation | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 314 | NULL Is Not an Ordinary Runtime Type | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 315 | Type Promotion | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 316 | String Coercion Policy | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 317 | Boolean Context | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 318 | SQL Three-Valued Logic | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 319 | Comparison and NULL | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 320 | Initial Cast Model | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 321 | Type Resolution API | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 322 | SQL Value Representation | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 323 | Lexer | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 324 | Lexer Token Classes | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 325 | Identifier Case Rules | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 326 | SQL String Literals | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 327 | Numeric Literal Lexing | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 328 | Comments | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 329 | Source Locations | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 330 | Parser Architecture | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 331 | Initial SQL Statement Set | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 332 | Initial SELECT Grammar Surface | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 333 | Subqueries | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 334 | AST Philosophy | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 335 | AST Ownership | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 336 | Expression Precedence | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 337 | Binder Responsibilities | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 338 | Binding Context / Scope | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 339 | Bound Column Reference | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 340 | Unqualified Column Resolution | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 341 | Qualified Column Resolution | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 342 | SELECT * Expansion | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 343 | Output Names | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 344 | Expression IR | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 345 | Expression Immutability | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 346 | Expression Ownership | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 347 | Operator Registry | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 348 | Function Registry | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 349 | Function Volatility | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 350 | Aggregate Expressions | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 351 | Aggregate Query Semantics | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 352 | HAVING | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 353 | ORDER BY Resolution | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 354 | LIMIT / OFFSET | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 355 | DISTINCT | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 356 | CASE | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 357 | IN List | 10 | MIGRATED_PASS10 | ARCHITECTURE.md |
| 358 | Parameters | 10 | MIGRATED_DEFERRED_PASS10 | ARCHITECTURE.md |
| 359 | Logical Plan Node Contract | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 360 | Logical Plan Slot Identity | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 361 | Initial Logical Operators | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 362 | LogicalGet | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 363 | LogicalValues | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 364 | LogicalFilter | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 365 | LogicalProject | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 366 | LogicalJoin | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 367 | Join Predicate Decomposition | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 368 | Outer Join Semantics | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 369 | LogicalAggregate | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 370 | LogicalSort | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 371 | LogicalLimit | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 372 | LogicalInsert | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 373 | LogicalUpdate | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 374 | LogicalDelete | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 375 | Hidden System Slots | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 376 | CREATE TABLE Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 377 | PRIMARY KEY Semantics | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 378 | CREATE INDEX Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 379 | CREATE INDEX Execution Semantics | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 380 | DROP Semantics | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 381 | DDL Concurrency | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 382 | Catalog Transaction Visibility | 11 | MIGRATED_WITH_COMPLETION_PASS11 | ARCHITECTURE.md |
| 383 | Statement Binding Snapshot | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 384 | INSERT Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 385 | Default Expressions | 11 | MIGRATED_WITH_GAP_PASS11 | ARCHITECTURE.md |
| 386 | UPDATE Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 387 | DELETE Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 388 | RETURNING | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 389 | Constant Folding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 390 | Boolean Simplification | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 391 | Predicate Pushdown | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 392 | Projection Pruning | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 393 | Expression Canonicalization | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 394 | Logical Join Graph | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 395 | Logical Plan Construction for SELECT | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 396 | SELECT Without FROM | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 397 | INNER JOIN Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 398 | LEFT JOIN Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 399 | CROSS JOIN | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 400 | NATURAL / USING JOIN | 11 | MIGRATED_DEFERRED_PASS11 | ARCHITECTURE.md |
| 401 | Subquery Binding | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 402 | Scalar Subquery Semantics | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 403 | EXISTS | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 404 | IN Subquery | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 405 | Common Table Expressions | 11 | MIGRATED_DEFERRED_PASS11 | ARCHITECTURE.md |
| 406 | Expression Evaluation Contract | 11 | MIGRATED_BOUNDARY_PASS11 | ARCHITECTURE.md |
| 407 | Expression Nullability Metadata | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 408 | Expression Lineage | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 409 | Logical Properties | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 410 | Constraint-Derived Logical Properties | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 411 | Logical Plan Validation | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 412 | EXPLAIN Logical Output | 11 | MIGRATED_CROSSCUTTING_PASS11 | ARCHITECTURE.md |
| 413 | Error Model | 11 | MIGRATED_CROSSCUTTING_PASS11 | ARCHITECTURE.md |
| 414 | Parser Error Recovery | 11 | MIGRATED_PASS11 | ARCHITECTURE.md |
| 415 | SQL Grammar Testing | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 416 | Binder Tests | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 417 | Type-System Property Tests | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 418 | Catalog Tests | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 419 | Logical Planner Tests | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 420 | Logical Rewrite Tests | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 421 | Catalog / Front-End Benchmarks | 11 | MIGRATED_PERFORMANCE_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 422 | Parser/AST Memory Benchmark | 11 | MIGRATED_PERFORMANCE_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 423 | Front-End Fuzzing | 11 | MIGRATED_VERIFICATION_PASS11 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 424 | Supported SQL v1 Target | 11 | MIGRATED_SCOPE_PASS11 | ARCHITECTURE.md |
| 425 | Deliberately Deferred SQL Features | 11 | MIGRATED_DEFERRED_PASS11 | ARCHITECTURE.md |
| 426 | High-Reward Future Front-End Features | 11 | MIGRATED_DEFERRED_PASS11 | ARCHITECTURE.md |
| 427 | Front-End / Logical-Layer Invariants | 11 | RECONCILED_PASS11 | ARCHITECTURE.md |
| 428 | Recommended Module Layout for the Upper Layer | 11 | CLASSIFIED_PASS11 | DEVELOPMENT.md |
| 429 | Implementation Order for Catalog + SQL Front End | 11 | CLASSIFIED_PASS11 | DEVELOPMENT.md |
| 430 | Upper-Layer Milestone 1 | 11 | CLASSIFIED_PASS11 | DEVELOPMENT.md |
| 431 | Upper-Layer Milestone 2 | 11 | CLASSIFIED_PASS11 | DEVELOPMENT.md |
| 432 | Upper-Layer Milestone 3 | 11 | CLASSIFIED_PASS11 | DEVELOPMENT.md |
| 433 | Architecture Status After Upper Semantic Layer | 11 | MIGRATED_WITH_CLASSIFICATION_PASS11 | ARCHITECTURE.md + PROJECT_STATE.md/devlog |
| 434 | Vectorized Physical Execution Engine Contract | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 435 | Physical Plan vs Runtime Operator State | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 436 | Physical Plan Node Contract | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 437 | Initial Physical Operators | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 438 | DataChunk | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 439 | Why 1024 Rows | 12 | MIGRATED_RATIONALE_PASS12 | ARCHITECTURE.md |
| 440 | Vector Kinds | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 441 | Flat Numeric Vector Layout | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 442 | Validity Mask | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 443 | SelectionVector | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 444 | Dictionary Composition | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 445 | UnifiedVectorFormat | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 446 | VARCHAR Execution Representation | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 447 | String Lifetime Rules | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 448 | DataChunk StringHeap | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 449 | Borrowed Vectors Inside a Pipeline | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 450 | Chunk Reuse | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 451 | Execution Row Layout | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 452 | RowCollection | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 453 | Query Arena | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 454 | QueryMemoryManager | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 455 | Memory Reservations | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 456 | Memory Pressure Protocol | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 457 | SpillManager | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 458 | Spill Block Format | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 459 | Spill I/O Pattern | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 460 | Expression Executor | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 461 | Vectorized Arithmetic Kernels | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 462 | Vectorized Comparison Kernels | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 463 | AND / OR Short-Circuiting | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 464 | Physical Pipeline Model | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 465 | Pipeline Roles | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 466 | Physical Plan to Pipeline Graph | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 467 | Pipeline Runtime Interfaces | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 468 | Single-Thread First, Parallel-Ready | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 469 | Pipeline Cancellation | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 470 | Sequential Scan | 12 | MIGRATED_WITH_INTEGRATION_PASS12 | ARCHITECTURE.md |
| 471 | Scan Page Guard Lifetime | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 472 | Scan Predicate Pushdown Boundary | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 473 | Index Scan | 12 | MIGRATED_WITH_INTEGRATION_PASS12 | ARCHITECTURE.md |
| 474 | RID Batching for Index Scan | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 475 | Index Order Property | 12 | MIGRATED_WITH_INTEGRATION_PASS12 | ARCHITECTURE.md |
| 476 | PhysicalFilter | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 477 | PhysicalProject | 12 | MIGRATED_PASS12 | ARCHITECTURE.md |
| 478 | PhysicalLimit | 12 | MIGRATED_WITH_COMPLETION_PASS12 | ARCHITECTURE.md |
| 479 | Nested-Loop Join | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 480 | Index Nested-Loop Join | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 481 | Hash Join Role | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 482 | Hash Join Build Storage | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 483 | Hash Join Hash Directory | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 484 | Why Directory + Duplicate Chains | 13 | MIGRATED_RATIONALE_PASS13 | ARCHITECTURE.md |
| 485 | Hash Function Contract | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 486 | Hash Join NULL Semantics | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 487 | Hash Join Build Pipeline | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 488 | Hash Join Probe State | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 489 | Hash Join Residual Predicate | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 490 | LEFT Hash Join | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 491 | Grace Hash Join Spilling | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 492 | Recursive Hash Repartition | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 493 | Hash Join Spill Ordering | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 494 | Hash Aggregate | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 495 | Aggregate Function State API | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 496 | Initial Aggregate Semantics | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 497 | Group Hash Table | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 498 | GROUP BY NULL Semantics | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 499 | Global Aggregate Fast Path | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 500 | Hash Aggregate Spill | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 501 | DISTINCT | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 502 | Sort Operator | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 503 | Sort Record | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 504 | Sort Comparison | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 505 | In-Memory Sort Algorithm | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 506 | External Merge Sort | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 507 | Sort Spill Run Format | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 508 | PhysicalTopN | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 509 | Merge Join | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 510 | DML Target Materialization | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 511 | Halloween Problem Protection | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 512 | DML Target Spool Memory and Spill | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 513 | DML Revalidation | 13 | MIGRATED_REFINEMENT_PASS13 | ARCHITECTURE.md |
| 514 | INSERT Execution | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 515 | UPDATE Execution | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 516 | DELETE Execution | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 517 | RETURNING Buffer | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 518 | Query Result Interface | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 519 | Physical DDL Execution | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 520 | Physical VACUUM | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 521 | Physical Operator Properties | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 522 | Ordering Property | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 523 | Pipeline Breaker Memory Semantics | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 524 | Pipeline Dependency DAG | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 525 | Worker Model | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 526 | Pipeline Morsels | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 527 | Local Operator State | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 528 | Parallel Sequential Scan | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 529 | Parallel Hash Join Build | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 530 | Parallel Hash Join Probe | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 531 | Parallel Hash Aggregate | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 532 | Parallel Sort | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 533 | Task Scheduler | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 534 | Query Fairness | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 535 | NUMA | 13 | MIGRATED_DEFERRED_PASS13 | ARCHITECTURE.md |
| 536 | SIMD Policy | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 537 | Branch Reduction | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 538 | Prefetch | 13 | MIGRATED_PASS13 | ARCHITECTURE.md |
| 539 | Query Profiling | 13 | MIGRATED_CROSSCUTTING_PASS13 | ARCHITECTURE.md |
| 540 | EXPLAIN ANALYZE | 13 | MIGRATED_CROSSCUTTING_PASS13 | ARCHITECTURE.md |
| 541 | Pipeline Profiling | 13 | MIGRATED_CROSSCUTTING_PASS13 | ARCHITECTURE.md |
| 542 | Physical Plan Validator | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md |
| 543 | Execution Error Model | 13 | MIGRATED_CROSSCUTTING_PASS13 | ARCHITECTURE.md |
| 544 | Integer Arithmetic Errors | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 545 | Division | 13 | MIGRATED_WITH_COMPLETION_PASS13 | ARCHITECTURE.md |
| 546 | Execution Testing Strategy | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 547 | Vector Correctness Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 548 | String Lifetime Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 549 | Hash Join Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 550 | Aggregate Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 551 | Sort Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 552 | DML Execution Tests | 13 | MIGRATED_VERIFICATION_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 553 | Execution Microbenchmarks | 13 | MIGRATED_PERFORMANCE_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 554 | Vector Size Benchmark | 13 | MIGRATED_PERFORMANCE_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 555 | End-to-End Execution Benchmarks | 13 | MIGRATED_PERFORMANCE_PASS13 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 556 | Deliberately Deferred Execution Features | 13 | MIGRATED_DEFERRED_PASS13 | ARCHITECTURE.md |
| 557 | High-Reward Future Execution Experiments | 13 | MIGRATED_DEFERRED_PASS13 | ARCHITECTURE.md |
| 558 | Practical Performance Rules | 13 | MIGRATED_PERFORMANCE_PASS13 | ARCHITECTURE.md |
| 559 | Execution Invariants | 13 | RECONCILED_PASS13 | ARCHITECTURE.md |
| 560 | Recommended Execution Module Layout | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 561 | Execution Implementation Order | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 562 | Execution Milestone 1 | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 563 | Execution Milestone 2 | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 564 | Execution Milestone 3 | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 565 | Execution Milestone 4 | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 566 | Execution Milestone 5 | 13 | CLASSIFIED_PASS13 | DEVELOPMENT.md |
| 567 | Architecture Status After Practical Execution Layer | 13 | MIGRATED_WITH_CLASSIFICATION_PASS13 | ARCHITECTURE.md + PROJECT_STATE.md/devlog |
| 568 | Cost-Based Optimizer Contract | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 569 | Optimizer Layering | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 570 | Optimizer Inputs | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 571 | Optimizer Output | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 572 | Statistics Philosophy | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 573 | ANALYZE | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 574 | Table Statistics | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 575 | Column Statistics | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 576 | Statistics Collection Strategy | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 577 | Small-Table Exact Statistics | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 578 | NDV Estimation | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 579 | Most-Common-Value Collection | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 580 | Histogram Collection | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 581 | Why Equi-Depth Histograms | 14 | MIGRATED_RATIONALE_PASS14 | ARCHITECTURE.md |
| 582 | Histogram Value Semantics | 14 | MIGRATED_INTEGRATION_PASS14 | ARCHITECTURE.md |
| 583 | Statistics Serialization | 14 | MIGRATED_WITH_GAP_PASS14 | ARCHITECTURE.md |
| 584 | Statistics Snapshot During Planning | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 585 | Statistics Freshness | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 586 | CardinalityEstimate | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 587 | Exact-Zero vs Estimated-Small | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 588 | Row Width Estimate | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 589 | Selectivity Range | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 590 | Equality Selectivity: Constant | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 591 | Equality Selectivity: Column to Column | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 592 | MCV-Aware Join Estimation | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 593 | Unique-Key Join Estimation | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 594 | Range Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 595 | IS NULL Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 596 | IN-List Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 597 | NOT Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 598 | PredicateTruthEstimate | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 599 | AND Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 600 | OR Selectivity | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 601 | Same-Column Constraint Set | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 602 | Correlated Columns | 14 | MIGRATED_DEFERRED_PASS14 | ARCHITECTURE.md |
| 603 | Cardinality Through Projection | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 604 | Cardinality Through Filter | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 605 | Cardinality Through Limit | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 606 | Cardinality Through DISTINCT | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 607 | GROUP BY Cardinality | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 608 | Multi-Column NDV Damping | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 609 | LEFT JOIN Cardinality | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 610 | Cost Model Philosophy | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 611 | Cost Structure | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 612 | Default Cost Units | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 613 | Cost Calibration Tool | 14 | MIGRATED_CROSSCUTTING_PASS14 | ARCHITECTURE.md |
| 614 | Buffer Cache Assumption | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 615 | Sequential Scan Cost | 14 | MIGRATED_WITH_INTEGRATION_PASS14 | ARCHITECTURE.md |
| 616 | B+ Tree Point Lookup Cost | 14 | MIGRATED_WITH_INTEGRATION_PASS14 | ARCHITECTURE.md |
| 617 | Index Range Scan Cost | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 618 | Index-Heap Correlation | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 619 | Fallback Heap Fetch Cost | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 620 | Access-Path Predicate Analysis | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 621 | Sargable B+ Tree Conditions | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 622 | Composite Index Bounds | 14 | MIGRATED_WITH_COMPLETION_PASS14 | ARCHITECTURE.md |
| 623 | Index Predicate Residuals | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 624 | Base Access Paths | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 625 | Multiple Indexes | 14 | MIGRATED_DEFERRED_PASS14 | ARCHITECTURE.md |
| 626 | Index Scan Break-Even | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 627 | Required Columns and Access Cost | 14 | MIGRATED_PASS14 | ARCHITECTURE.md |
| 628 | Physical Property Model | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 629 | Ordering Satisfaction | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 630 | Interesting Orders | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 631 | Plan Alternative Key | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 632 | RelationSet | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 633 | Join Graph | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 634 | Join Enumeration Scope | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 635 | Bushy Dynamic Programming | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 636 | Exhaustive Join Threshold | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 637 | Large-Join Heuristic | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 638 | Cartesian Products | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 639 | Join Algorithm Alternatives | 15 | MIGRATED_WITH_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 640 | Hash Join Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 641 | Hash Join Memory Estimate | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 642 | Hash Join Spill Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 643 | Nested Loop Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 644 | Index Nested-Loop Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 645 | Repeated INLJ Key Locality | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 646 | Merge Join Cost | 15 | MIGRATED_WITH_CAPABILITY_GATE_PASS15 | ARCHITECTURE.md |
| 647 | Sort Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 648 | TopN Cost | 15 | MIGRATED_WITH_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 649 | Hash Aggregate Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 650 | Sort Aggregate Cost | 15 | MIGRATED_WITH_CAPABILITY_GATE_PASS15 | ARCHITECTURE.md |
| 651 | DISTINCT Cost | 15 | MIGRATED_WITH_CAPABILITY_GATE_PASS15 | ARCHITECTURE.md |
| 652 | Sort Enforcement | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 653 | Final ORDER BY Optimization | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 654 | Limit-Aware Planning | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 655 | Required Rows Objective | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 656 | Predicate CPU Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 657 | Filter Predicate Ordering | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 658 | Physical Plan Memo | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 659 | Dominance Rule | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 660 | Join DP Initialization | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 661 | Join DP Transition | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 662 | Join Cardinality Must Be Algorithm-Independent | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 663 | Outer Join Search Constraints | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 664 | Subquery Physical Planning | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 665 | Parameter-Free Plans | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 666 | Statistics Missing Fallbacks | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 667 | Statistics Confidence | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 668 | Estimation Provenance | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 669 | Optimizer Trace | 15 | MIGRATED_CROSSCUTTING_PASS15 | ARCHITECTURE.md |
| 670 | EXPLAIN Estimates | 15 | MIGRATED_CROSSCUTTING_PASS15 | ARCHITECTURE.md |
| 671 | EXPLAIN ANALYZE Estimate Error | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 672 | Cardinality Error Attribution | 15 | MIGRATED_CROSSCUTTING_PASS15 | ARCHITECTURE.md |
| 673 | No Runtime Statistics Feedback in v1 | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 674 | Planner Time Budget | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 675 | Planning Memory Budget | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 676 | Deterministic Optimization | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 677 | Cost Tie Tolerance | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 678 | Plan Fingerprint | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 679 | Logical Rewrite Before Costing | 15 | MIGRATED_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 680 | Join Predicate Equivalence Classes | 15 | MIGRATED_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 681 | Constant Propagation Through Equality | 15 | MIGRATED_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 682 | Contradiction Detection | 15 | MIGRATED_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 683 | Unique/Key Metadata in Optimization | 15 | MIGRATED_INTEGRATION_PASS15 | ARCHITECTURE.md |
| 684 | Foreign-Key Metadata | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 685 | Join Elimination | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 686 | Index-Order Match | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 687 | Reverse-Scan Limitation | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 688 | Hash Join Build-Side Selection | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 689 | Join Output Width | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 690 | Hash Table Payload Pruning | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 691 | Sort Payload Pruning | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 692 | Aggregate State Width | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 693 | Spill Probability | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 694 | Query Memory Allocation During Planning | 15 | MIGRATED_WITH_COMPLETION_PASS15 | ARCHITECTURE.md |
| 695 | Pipeline-Aware Peak Memory | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 696 | Materialization Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 697 | Startup vs Total Cost | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 698 | Plan Search and LIMIT | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 699 | Optimizer Correctness Boundary | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 700 | Optimizer Validation | 15 | MIGRATED_PASS15 | ARCHITECTURE.md |
| 701 | Statistics Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 702 | Selectivity Estimation Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 703 | Join Estimation Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 704 | Access Path Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 705 | Join-Order Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 706 | Interesting-Order Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 707 | Memory/Spill Plan Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 708 | Optimizer Differential Correctness Tests | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 709 | Optimizer Fuzzing | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 710 | Cost Model Benchmarks | 15 | MIGRATED_PERFORMANCE_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 711 | Plan Regression Suite | 15 | MIGRATED_VERIFICATION_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 712 | Optimizer Performance Benchmarks | 15 | MIGRATED_PERFORMANCE_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 713 | Star Schema Benchmark | 15 | MIGRATED_PERFORMANCE_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 714 | No Benchmark Gaming | 15 | MIGRATED_INVARIANT_PASS15 | ARCHITECTURE.md §41/§42 + VERIFICATION.md |
| 715 | Deliberately Deferred Optimizer Features | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 716 | High-Reward Future Optimizer Experiments | 15 | MIGRATED_DEFERRED_PASS15 | ARCHITECTURE.md |
| 717 | Cost-Based Optimizer Invariants | 15 | RECONCILED_PASS15 | ARCHITECTURE.md |
| 718 | Recommended Optimizer Module Layout | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 719 | Optimizer Implementation Order | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 720 | Optimizer Milestone 1 | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 721 | Optimizer Milestone 2 | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 722 | Optimizer Milestone 3 | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 723 | Optimizer Milestone 4 | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 724 | Optimizer Milestone 5 | 15 | CLASSIFIED_PASS15 | DEVELOPMENT.md |
| 725 | Complete Core Architecture Status | 15 | MIGRATED_WITH_CLASSIFICATION_PASS15 | ARCHITECTURE.md + PROJECT_STATE.md/devlog |

## Final totals

- `ARCHITECTURE.md`: 639
- `ARCHITECTURE.md` + `VERIFICATION.md`: 50
- `DEVELOPMENT.md`: 32
- architecture/current-state/history split: 4
- `AGENTS.md`: 1
- **Total: 726**

`PENDING = 0`.
