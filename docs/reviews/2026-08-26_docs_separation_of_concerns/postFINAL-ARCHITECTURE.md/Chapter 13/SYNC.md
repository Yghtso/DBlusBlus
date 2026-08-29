# Chapter 13 database.control verification synchronization — COMPLETE

The independent byte-exact `database.control` methodology is now documented in [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:4212).

- V13-1: **CLOSED**
- Frozen architecture semantic questions: **NONE**
- Chapter 13: **FULLY REVIEWED AND CLOSED**
- Chapter 14 review: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**

## Repository baseline and scope

- Initial status: clean
- Initial index: clean
- Initial HEAD: `991ea73f4f40614e44ac261d076bdfa01a5ea882`
- Pre-existing material: none reported by Git
- Only task-modified file: `docs/VERIFICATION.md`
- `ARCHITECTURE.md`, development/project-state documentation, source, tests, build files, devlogs, and review artifacts were untouched.

## Verification organization

The existing “Control slots and required recovery inputs” subsection now contains:

- independent byte/endian/CRC/decode oracles;
- distinctive valid-slot fixture;
- canonical initial-file fixture;
- field, checkpoint, and high-water procedures;
- CRC and format-classification matrices;
- slot-selection and torn-update matrix;
- exhaustion/checkpoint cross-owner mappings;
- 82-item atomic obligation coverage map.

This preserves existing ownership: byte methodology is local, while torn-update crashes, allocator exhaustion, WAL checkpoint records, and checkpoint installation remain with their established procedural owners.

## Exact file and slot framing

- Filename: `database.control`
- File size: exactly 8192 bytes
- Slot 0: bytes `0..4095`
- Slot 1: bytes `4096..8191`
- Each slot: exactly 4096 bytes
- Standalone 4095/4097-byte slots and 8191/8193-byte files are rejected as noncanonical framing.

### Complete distinctive slot vector

| Offset | Width | Field | Fixture bytes |
|---:|---:|---|---|
| 0 | 8 | `DBLUSCTL` | `44 42 4C 55 53 43 54 4C` |
| 8 | 2 | `format_version=1` | `01 00` |
| 10 | 2 | `header_size=88` | `58 00` |
| 12 | 4 | `flags=0` | `00 00 00 00` |
| 16 | 8 | `generation=0x0102030405060708` | `08 07 06 05 04 03 02 01` |
| 24 | 8 | `latest_checkpoint_lsn=0x11223348` | `48 33 22 11 00 00 00 00` |
| 32 | 8 | `latest_checkpoint_end_lsn=0x22334458` | `58 44 33 22 00 00 00 00` |
| 40 | 8 | `checkpoint_redo_lsn=0x01020308` | `08 03 02 01 00 00 00 00` |
| 48 | 8 | `reserved_txn_id_end=1,048,578` | `02 00 10 00 00 00 00 00` |
| 56 | 8 | `txn_status_reclaim_before=97,922` | `82 7E 01 00 00 00 00 00` |
| 64 | 4 | `next_file_id=0x12345678` | `78 56 34 12` |
| 68 | 4 | reserved zero | `00 00 00 00` |
| 72 | 8 | `next_catalog_object_id=0x1122334455667788` | `88 77 66 55 44 33 22 11` |
| 80 | 4 | CRC32C `0x9673EF23` | `23 EF 73 96` |
| 84 | 4 | reserved zero | `00 00 00 00` |
| 88 | 4008 | reserved suffix | all zero through byte 4095 |

## Independent oracles

The methodology requires:

- Test-side byte construction using explicit shifts, without production encoders or host structs.
- Independent little-endian comparison for every uint16, uint32, and uint64.
- Independent semantic decode and comparison with chosen fixture values.
- Independent CRC32C checked first against `CRC32C("123456789") = 0xE3069283`.
- CRC coverage over all 4096 bytes with offsets `80..83` logically zero.
- Stored CRC verification in little-endian form.
- Production encode/decode round-trip treated only as supplementary coverage.

The distinctive fixture CRC is `0x9673EF23`. The canonical initial-slot CRC is `0x530BD55D`, encoded `5D D5 0B 53`.

## Canonical initial file

- Slot 0:
  - generation `1`
  - checkpoint triplet all zero
  - `reserved_txn_id_end=2`
  - `txn_status_reclaim_before=2`
  - `next_file_id=1`
  - `next_catalog_object_id=1`
  - CRC32C `0x530BD55D`
  - all reserved bytes zero
- Slot 1: all 4096 bytes zero
- Selected authority: slot 0, generation 1
- Zero slot 1 is a noncandidate and does not invalidate the valid initial file.

## Field and classification methodology

Covered independently:

- Exact magic, including changed, shifted, lowercase, and embedded-NUL variants.
- Version 1 positive case.
- Version 0 invalid/corrupt candidate.
- Positive future version fail-closed as `UNSUPPORTED_DATABASE_FORMAT`.
- Future-format slot plus older v1 slot: no downgrade fallback.
- Wrong-magic candidate plus valid v1 slot: ordinary fallback remains permitted.
- Header sizes `0`, `87`, `88`, `89`, and another representable value.
- Nonzero flags.
- Generation zero and generation exhaustion/no-wrap.
- Both reserved32 fields.
- First, middle, and final bytes of the 4008-byte suffix.
- CRC errors and CRC-valid semantic corruption.

Every semantic-invalid fixture receives a recomputed valid CRC, ensuring the semantic validator—not checksum failure—is isolated.

## Checkpoint methodology

The procedure separates:

1. Structural slot validity:
   - exactly three zero fields for no checkpoint; or
   - all three nonzero with BEGIN not after END.

2. Recovery usability:
   - valid retained BEGIN/DATA/END records;
   - matching checkpoint identity;
   - complete contiguous chunks and totals;
   - valid framing and CRC;
   - valid redo bound;
   - record-start alignment and valid WAL range.

A newer unusable checkpoint-bearing slot may fall back only to an older slot whose complete recovery inputs remain retained and valid.

## High-water methodology

Covered states include:

- TxnId initial end `2`, normal exact block end `1,048,578`, and terminal maximum `18,446,744,073,708,503,042`.
- TxnId below-minimum rejection and durable-reservation nonreuse.
- Reclaim cutoff minimum, aligned normal value, maximum aligned value, below-minimum, misaligned, and beyond-reservation cases.
- `next_file_id` initial, normal, `UINT32_MAX` exhausted-next state, and zero rejection.
- `next_catalog_object_id` initial, normal, `UINT64_MAX` exhausted-next state, and zero rejection.
- Shared TableId/IndexId/ConstraintId allocation and consumed crash gaps.

Full Chapter-14 reclamation proof was not added; only Chapter-13 slot-level bounds and cross-references are covered.

## CRC matrix

The matrix covers:

- canonical bytes and exact expected CRC;
- covered-field bit flip with unchanged CRC;
- stored-CRC bit flip;
- semantic-invalid slot with recomputed CRC;
- suffix-invalid slot with recomputed CRC;
- incorrect self-referential CRC calculation that includes stored checksum bytes.

## Slot-selection methodology

Explicit cases now include:

- slot 0 valid / slot 1 zero;
- both slots valid with different generations in both physical orders;
- one valid / one CRC-invalid;
- one valid / one semantic-invalid;
- both invalid;
- equal-generation identical bytes: equivalent semantic authority;
- equal-generation differing bytes: corruption;
- future-format slot plus older v1: unsupported, no fallback;
- wrong-magic slot plus valid v1: valid v1 selected;
- checkpoint-unusable newer slot plus older candidate: conditional retained-object fallback.

## Existing coverage mappings

- Alternating and torn writes: persistent high-water crash procedure and Crash Injection Framework.
- Generation exhaustion: Numeric Exhaustion control-generation procedure.
- FileId exhaustion: FileId specialization.
- Catalog-object exhaustion: shared catalog-object specialization.
- TxnId block reservation/nonreuse: TxnId terminal-block and crash procedures.
- Checkpoint records: WAL persistent checkpoint codecs.
- Checkpoint installation: checkpoint crash boundaries and lifecycle procedures.

No existing control, checkpoint, WAL, or lifecycle procedure was weakened.

## Obligation inventory and coverage

The complete map is at [Atomic control-file obligation coverage map](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:4471).

| Domain | IDs | Count |
|---|---:|---:|
| File framing | 1–5 | 5 |
| Slot fields | 6–21 | 16 |
| Endian/independent decode | 22–26 | 5 |
| CRC | 27–32 | 6 |
| Version/classification | 33–37 | 5 |
| Selection/generation | 38–44 | 7 |
| Checkpoint relations | 45–49 | 5 |
| Initial state | 50–51 | 2 |
| High-water fields | 52–60 | 9 |
| Candidate selection | 61–63 | 3 |
| Alternating publication | 64–72 | 9 |
| Exhaustion | 73–76 | 4 |
| Update frequency | 77–78 | 2 |
| Checkpoint publication | 79–80 | 2 |
| Framing/error distinctions | 81–82 | 2 |
| **Total** |  | **82** |

Coverage totals:

- COMPLETE: **82**
- PARTIAL: **0**
- MISSING: **0**
- CONTRADICTORY: **0**

## Final reread answers

Questions 1–88: **YES**
Question 89, “Did any new semantic rule get invented?”: **NO**
Question 90, “Is V13-1 fully closed by this methodology?”: **YES**

Documentation-model answers:

- A. Current implementation-state narration introduced? **NO**
- B. DEVELOPMENT sequencing introduced? **NO**
- C. Devlog/history introduced? **NO**
- D. Architecture unnecessarily duplicated? **NO**
- E. ARCHITECTURE.md modified? **NO**
- F. All task-created sections procedural/analytical? **YES**
- G. Independent of recovery implementation progress? **YES**
- H. Time-independent? **YES**
- I. Valid persistent/runtime temporal language preserved? **YES**
- J. Separation of concerns preserved? **YES**

## Diff classification

One contiguous logical hunk was created under the existing control-slot owner:

- A–I: byte oracle, field matrix, CRC, initial state, negative fixtures, checkpoint/high-water/selection/future-format methodology.
- J–K: torn-update and exhaustion mappings.
- L: mandatory matrices.
- M: 82-item atomic inventory.
- N: precise navigation and cross-references.
- O: only local wrapping adjacent to the replaced introductory paragraph.

No unrelated cleanup occurred.

## Final Git state

- Final status: ` M docs/VERIFICATION.md`
- Final index: clean
- Final HEAD: `991ea73f4f40614e44ac261d076bdfa01a5ea882`
- `git diff --check`: passed
- External repository changes during task: none detected
- No pre-existing material was modified or staged
- No build, test, benchmark, staging, commit, implementation work, devlog, or review artifact was created

Out-of-scope wording remains unchanged:

- §14.17
- §§15.7.2–15.7.3
- §31.7
- Appendix C

Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.