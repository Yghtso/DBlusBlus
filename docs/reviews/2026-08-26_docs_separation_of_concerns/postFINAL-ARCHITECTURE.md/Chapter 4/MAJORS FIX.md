# Architecture revision verdict

1. **FROZEN ARCHITECTURE REVISION — ACCEPTED D-M1 + D-M2 INTEGRATED.**

D-M1 now completes the root-publication ownership/adoption contract. D-M2 now completes the already-supported offline whole-database removal contract. The authoritative architecture remains v1.

## Repository state

2. Initial state:

```text
HEAD: 1f40c432f147a0b717ac80789d62a7779ea2df44
Index: empty
Working tree:
?? docs/reviews/.../Chapter 4/D-M1(0).md
?? docs/reviews/.../Chapter 4/D-M2(0).md
```

3. Both pre-existing review artifacts were preserved unchanged and unstaged.

4. Sections changed:

- §2.5: one necessary namespace cross-reference.
- [§3.3.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:368)
- [§3.3.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:458)
- [§3.3.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:545)
- [§3.3.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:658)
- [§4.7.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1317)
- [§4.7.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1523)
- [§4.7.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1573)
- New [§4.7.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1642)
- [§4.15](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:2719)
- [§41.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24324)
- [Appendix B](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25077)

5. All other architecture sections and every other file were intentionally left unchanged.

## D-M1 result

6. Final invariant: creation publication, ordinary open, and offline removal use one authority—the stable process-local root/control-inode claim plus the ordinary exclusive `database.control` lock.

7. Creator acquisition occurs after complete staged-bootstrap validation and before final-root rename.

8. Same-filesystem directory rename preserves root inode, control inode, retained descriptors, process claim, and held control lock.

9. Root durability remains final-root rename plus successful external-parent `fsync`. The lock is only an admission gate.

10. Returned-handle creation continuously carries the same ownership into recovery and READY; no release/reacquire race exists.

11. No-handle creation releases the control lock, then removes the process claim, only after durable publication and before returning success.

12. Competing same-process or cross-process open/removal receives `DATABASE_BUSY` before control-slot, WAL, recovery, catalog, or database mutation work.

13. An independent next owner must verify final parent-entry identity, synchronize the external parent, and revalidate identity before inspection, recovery, removal, READY, or acknowledged work.

14. If post-rename parent `fsync` fails, creation reports no success, publishes no READY handle, retains ownership while reconciling, and treats durability as uncertain. After process death, the next owner performs adoption.

15. No persistent marker, control bit, lifecycle state, sidecar lock, or new lock identity was introduced.

## D-M2 result

16. Removal is offline-only: no ordinary `DatabaseInstance`, transaction, BufferPool/database manager, worker, or background producer may exist.

17. The remover acquires the same stable process claim and ordinary nonblocking control lock.

18. D-M1 parent-entry adoption is mandatory before destructive validation or mutation.

19. Destructive identity requires a retained no-follow root, regular no-follow `database.control`, ownership, adoption, and at least one complete control slot with exact:

```text
bytes 0..7    = ASCII DBLUSCTL
bytes 8..9    = format_version 1
bytes 10..11  = header_size 88
```

20. A recognized supported-v1 root remains removable despite CRC, generation, range, flag/reserved, cross-field, catalog, WAL, or required-file corruption.

21. Missing control, wrong type, unreadable/unrecognizable framing, or an unrelated directory is refused without destructive mutation.

22. Exact control-family framing with `format_version > 1` returns `UNSUPPORTED_DATABASE_FORMAT`; v1 does not force-remove future formats.

23. U1 is canonical: after supported-v1 proof, exclusive ownership, adoption, and durable retirement, cleanup may delete all entries contained beneath the retained retired root, including unknown names.

24. Ordinary orphan cleanup remains stricter and cannot guess or delete unknown entries.

25. Cleanup is descriptor-relative and no-follow; it cannot follow symlinks, cross unproven mount/filesystem boundaries, or touch external-parent siblings.

26. Retired-root grammar:

```text
D.dblusblus-removing-<32 lowercase hexadecimal digits>
```

The token represents a collision-resistant 128-bit value. Reserved private grammars cannot themselves be final database basenames.

27. Retirement uses same-parent, same-filesystem, no-replace rename. Collision selects another token.

28. Removal publication is retirement rename plus successful external-parent `fsync`.

29. Success means the captured old database is durably absent from final name `D`; physical reclamation need not be complete.

30. Cleanup is private, contained, bottom-up, retryable, and may be immediate or deferred.

31. Cleanup failure after publication leaves recognized residue but does not reverse semantic success.

32. A distinct database may be recreated at `D` after durable retirement while old residue remains.

33. The old control lock and claim remain held through retirement publication. They may continue through immediate cleanup or be released afterward. `database.control` cannot be unlinked while old `D` remains live.

## Canonical protocols

34. Final root-creation sequence:

1. Create staging root no-follow.
2. Create `pending/` and `wal/`.
3. Initialize all required bootstrap files.
4. Synchronize startup-critical regular files.
5. Synchronize staged directories/root.
6. Fully validate bootstrap.
7. Establish stable process claim and acquire ordinary control lock.
8. Verify final `D` absent, synchronize its external parent, and revalidate absence.
9. Rename staging root to `D` without replacement.
10. Synchronize the external parent.
11. Publish durable creation; continue ownership into a returned handle or safely release it before no-handle success.

35. Final whole-root-removal sequence:

1. Require offline/CLOSED operation.
2. Retain external parent and open exact `D` no-follow.
3. Open regular no-follow `database.control`.
4. Establish stable process claim.
5. Acquire the ordinary control lock.
6. Perform D-M1 adoption.
7. Prove supported-v1 destructive identity.
8. Select an absent exact retired sibling.
9. Rename `D` to that sibling without replacement.
10. Delete no content yet.
11. Synchronize the external parent.
12. Publish semantic removal.
13. Optionally reclaim the retired tree under U1.
14. If fully reclaimed, `rmdir` the retired root and synchronize the external parent.
15. Release ownership resources in safe order.

## Crash and uncertainty

36. Creation crash prefixes:

- Before rename: staging only; never an open target.
- After rename/before parent sync: no ordinary work; creator ownership excludes competitors; a surviving final root requires next-owner adoption.
- After parent sync: final root is durably published.

37. Removal crash prefixes:

- Before retirement rename: complete live `D`.
- After rename/before parent sync: no content deletion and no success; crash yields live `D` or recognized retired root.
- After parent sync: old `D` is durably absent.
- During cleanup: retryable private residue only.
- After retired-root `rmdir` but before final sync: residue may reappear; semantic removal remains complete.
- After final sync: physical reclamation is durable.

38. Parent-sync failure never proves success or failure of the preceding rename. The live owner retains authority while reconciling; after owner death, the next actor classifies/adopts the durable namespace before use.

## Final invariants

39. Root ownership: create, open, and remove share one stable claim plus control lock.

40. Root publication: the final root becomes durable only through rename plus external-parent synchronization.

41. Removal publication: captured old `D` becomes durably absent only through retirement rename plus external-parent synchronization.

42. Control lock: the lock inode cannot be removed/replaced/recreated while its old root remains live.

43. Unknown entries: ordinary orphan cleanup preserves unknown names; explicit contained reclamation under a durably retired supported-v1 root may remove them.

44. Crash classification: no crash prefix exposes a partially deleted old database under live `D`.

## Section summaries

| Item | Section | Result |
|---:|---|---|
| 45 | §3.3.2 | Defines one shared creator/open/remover authority, rename continuity, adoption, and lock lifetime. |
| 46 | §3.3.3 | Inserts adoption immediately after ownership and before control/WAL/recovery inspection. |
| 47 | §3.3.4 | Adds established final-root parent durability as a READY prerequisite. |
| 48 | §3.3.7 | Summarizes mandatory create ownership and canonical offline removal. |
| 49 | §4.7.1 | Adds retired-root grammar, external-parent retention, reserved-name, and containment rules. |
| 50 | §4.7.6 | Separates strict ordinary orphan cleanup from explicit U1 whole-root cleanup. |
| 51 | §4.7.8 | Defines complete creator ownership, publication, returned/no-handle, and failure rules. |
| 52 | §4.7.9 | Canonically owns whole-database retirement, identity validation, publication, reclamation, and crash semantics. |
| 53 | §4.15 | Adds concise root ownership/publication/removal/control/U1 invariants. |
| 54 | Appendix B | Extends global one-owner and acknowledged-namespace-durability invariants. |
| 55 | §41.3 | Adds architecture-level competition, crash, adoption, removal, containment, and cleanup verification obligations. |

56. All new and modified cross-references resolve to existing canonical owners. No stale whole-root-removal reference to §4.7.8 remains.

57. Terminology is consistent across final root, creation staging root, retired root, root adoption, root/removal publication, physical reclamation, process claim, control lock, and external parent.

58. Targeted architecture self-review found the integrated contract internally coherent and all acceptance conditions satisfied.

## Semantic consistency answers

59.

| Question | Answer |
|---|---|
| Competing opener reaches recovery/READY during create publication? | **NO** |
| Acknowledged work relies on unsynchronized final-root entry? | **NO** |
| Creator death implies publication succeeded? | **NO** |
| Independent owner establishes parent durability before use? | **YES** |
| Removal begins while creator/open owner holds ownership? | **NO** |
| Removal deletes content before durable retirement? | **NO** |
| Control inode unlinked while old `D` is live? | **NO** |
| Crash leaves partially deleted old DB under live `D`? | **NO** |
| Retired root opens normally? | **NO** |
| Recreation allowed after durable removal with residue present? | **YES** |
| Cleanup failure resurrects old `D`? | **NO** |
| Ordinary orphan cleanup deletes arbitrary unknown names? | **NO** |
| Explicit retired-root cleanup may delete unknown contained entries? | **YES** |
| V1 may delete unsupported future-format DB? | **NO** |

60. No frozen-architecture follow-up question arose.

61. No semantic decision beyond the accepted D-M1/D-M2 design was introduced. Reserved-name exclusion and prior-absence synchronization are direct requirements of those accepted protocols.

62. Status remains **Authoritative v1 architecture contract**; architecture version remains **v1**.

## Pending findings and regressions

63. D-m1 remains pending; “heap geometry currently limits…” is unchanged.

64. D-m2 remains pending; §4.3.2.6 verification-procedure ownership was untouched.

65. D-m3 remains pending; “all six system-relation heap/FSM files” is unchanged.

66. The §7.5 “once the buffer layer exists” observation is unchanged.

67. Pass A regression: canonical v1 and parallel-ready language remain.

68. Pass B regression: §2.1 flow, immutable metadata, provider/consumer layering, and timeless BufferPool rule remain. Only the namespace cross-reference endpoint expanded to §4.7.9.

69. Pass C regression: v1 platform language, architecture-revision authority, `DRAINING -> CLOSING`, `CLOSING -> NONCONTINUABLE`, and failed-open distinctions remain.

70. `PROJECT_STATE.md` was not edited. D-M1 implementation relevance is not explicitly recorded; D-M2 remains future/unimplemented given the current implementation boundary. Existing implementation mismatches were not repaired.

71. `VERIFICATION.md` now requires a follow-up synchronization task for the new §41.3 D-M1/D-M2 obligations.

## Diff audit

72. Files changed:

```text
docs/ARCHITECTURE.md | 332 insertions, 54 deletions
```

73. Per-hunk classification:

| Hunk | Region | Class |
|---:|---|---|
| 1 | §2.5 cross-reference | K |
| 2 | §3.3.2 shared authority | A |
| 3 | §3.3.2 creator staging lock | A |
| 4 | §3.3.2 pre-lock creator distinction | A |
| 5 | §3.3.2 claim/rename/adoption | B |
| 6 | §3.3.2 ownership lifetime | D |
| 7 | §3.3.3 retired-target rejection | E |
| 8 | §3.3.3 ordered adoption | B |
| 9 | §3.3.4 READY prerequisite | C |
| 10 | §3.3.7 create outcome | C |
| 11 | §3.3.7 removal summary | D |
| 12 | §4.7.1 retired grammar | E |
| 13 | §4.7.1 parent retention | B |
| 14 | §4.7.1 containment | G |
| 15 | §4.7.6 U1 distinction | G |
| 16 | §4.7.8 ordered creation | A |
| 17 | §4.7.8 outcomes/failure | C |
| 18 | §4.7.9 canonical removal | F |
| 19 | §4.15 invariants | H |
| 20 | §41.3 obligations | J |
| 21–23 | Appendix B invariants | I |

74. Only `docs/ARCHITECTURE.md` was task-modified.

## Final Git state

75. Final state:

```text
HEAD: 1f40c432f147a0b717ac80789d62a7779ea2df44
Index: empty
Working tree:
 M docs/ARCHITECTURE.md
?? docs/reviews/.../Chapter 4/D-M1(0).md
?? docs/reviews/.../Chapter 4/D-M2(0).md
```

76. `git diff --check`: **passed with no output**.

77. Repository state changed only through the authorized architecture edit. HEAD and index did not change.

78. No external/user material was modified, removed, or staged.

79. No implementation, source, test, build, benchmark, or formatting work occurred.

80. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.1