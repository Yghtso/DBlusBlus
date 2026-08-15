# 0027 — PageFile Lifetime Contract Documentation

- Date: 2026-08-15
- Milestone: Pre-Phase-2 sanity finding F3 — `PageFile`/`DiskManager` lifetime contract

## Scope

Documented the existing concrete C++ ownership, registration, move, and destruction contract at
the public `PageFile` API boundary.

This task changed comments and current-state documentation only. It did not change runtime
behavior, ownership representation, registration behavior, architecture, tests, or any Phase 2
subsystem.

## Architecture and guidance followed

- `docs/ARCHITECTURE.md` §7.3, `DiskManager` responsibility and file-registration boundary
- `docs/ARCHITECTURE.md` §7.5, future BufferPool resident-page ownership boundary
- `docs/DEVELOPMENT.md` Phase 1/Phase 2 sequencing, which keeps `PageFile` in the
  BufferPool-independent foundation and defers buffered page lifetime to Phase 2

The architecture already defines the correct subsystem boundary and was not modified.

## Actual behavior inspected

`PageFile::Create` and `PageFile::Open` register a `FileId` in a caller-supplied `DiskManager` and,
on success, return a `PageFile` containing a non-owning pointer to that manager.

The implementation is move-only:

- move construction transfers the manager pointer with `std::exchange` and leaves the source
  pointer null,
- move assignment first releases the destination's existing registration, then transfers the
  source pointer and leaves the source pointer null,
- a moved-from object owns no registration, performs no destructor cleanup, and `AllocatePage`
  reports `PageFileErrorCode::NOT_OPEN`.

`PageFile::~PageFile` calls its private `Release`, which asks the referenced `DiskManager` to close
the managed `FileId`, then clears the pointer. The destructor cannot report and intentionally
ignores the `CloseFile` status. This closes the process-local registration and descriptor; it does
not delete the underlying file.

Factory failures clean any registration established before the failure and return no live
`PageFile`.

## Public contract documented

The class documentation in `src/storage/page_file.h` now states explicitly:

- `PageFile` does not own `DiskManager`,
- the supplied `DiskManager` must outlive every successful `PageFile` created or opened through it,
- external code must not close, unregister, or rebind the managed FileId while the `PageFile` is
  alive,
- `PageFile` retains normal registration cleanup responsibility,
- destruction closes the managed registration but does not delete the file,
- moving transfers cleanup responsibility,
- move assignment first releases the destination registration,
- the moved-from object is non-owning and inert for cleanup, and allocation reports `NOT_OPEN`.

The non-owning pointer member is also labeled with its null-after-move/release state.

## Files changed

- `src/storage/page_file.h`
  - documented the concrete public lifetime and registration contract
- `docs/PROJECT_STATE.md`
  - advanced the checkpoint, recorded the explicit contract, and marked F1/F2/F3 closed
- `devlog/0027-page-file-lifetime-contract.md`
  - recorded this milestone

No production `.cpp` implementation, test, older devlog, architecture document, development guide,
or verification guide changed.

## Existing test support

Existing `PageFileTest` coverage was inspected rather than changed:

- `ReopensValidatedFileAndRetainsPageCountAfterAllocations` destroys a live `PageFile`, then opens
  the same FileId again through the same manager, establishing destructor registration cleanup.
- successful `Create`/`Open` results move the local `PageFile` into `std::optional`; subsequent
  allocation and manager access establish that the moved-from local does not close the transferred
  registration.
- corruption, identity, missing-superblock, and post-creation failure tests establish cleanup of
  registrations on factory failure.
- independent-file tests establish separate registration responsibility per live `PageFile`.

No new behavioral test was necessary because the task documents existing, already exercised
behavior and makes no runtime change.

## Verification

- `cmake --build --preset clang-debug`: passed; all dependents of the changed header rebuilt
  successfully.
- `ctest --test-dir build/clang-debug -R '^PageFileTest\.' --output-on-failure`: 12/12 passed.
- `ctest --preset clang-debug --output-on-failure`: 209/209 passed.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without diagnostics.
- `clang-format --dry-run --Werror src/storage/page_file.h`: passed.
- `git diff --check`: passed.

GCC and ASan/UBSan suites were not rerun because the only C++ change is comments in a header and
runtime behavior and compiled declarations are unchanged. The current 209/209 GCC and ASan/UBSan
checkpoint from milestone 0026 remains the latest sanitizer/cross-compiler result.

## Pre-Phase-2 status

The pre-Phase-2 sanity findings are now closed:

```text
F1  HEAP_DATA ordinary PageNo validation                  closed
F2  concurrent DiskManager::ExtendFile verification      closed
F3  PageFile/DiskManager lifetime contract documentation closed
```

Implementation Phase 2 was not entered.

## Architecture questions discovered

None.
