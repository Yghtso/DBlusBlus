# Archived reviews

Earlier architecture, semantic and documentation reviews produced about 20 MB of working
material: pass-by-pass rewrite notes, finding reports, and old copies of the documents. None of
it is authoritative; the live documents (`ARCHITECTURE.md`, `PROJECT_STATE.md`,
`DEVELOPMENT.md`, `VERIFICATION.md`) already contain every accepted outcome, and `devlog/`
records the history.

The material was removed from the working tree on 2026-10-21 to keep the repository and the
documentation book small. It is preserved unchanged at the git tag
[`reviews-archive-2026-10-21`](https://github.com/Yghtso/DBlusBlus/tree/reviews-archive-2026-10-21/docs/reviews).
To get a local copy:

```sh
git fetch origin tag reviews-archive-2026-10-21
git restore --source reviews-archive-2026-10-21 -- docs/reviews
```

| Review | Contents | Size |
|---|---|---|
| [`2026-08-15_architecture`](https://github.com/Yghtso/DBlusBlus/tree/reviews-archive-2026-10-21/docs/reviews/2026-08-15_architecture) | The architecture rewrite in 17 passes (`pass0` to `pass16`), the rewrite plan, review rounds, and the pre-rewrite `ARCHITECTURE.md`, `AGENTS.md` and `PROJECT_STATE.md`. | 132 files, 16 MB |
| [`2026-08-15_semantic`](https://github.com/Yghtso/DBlusBlus/tree/reviews-archive-2026-10-21/docs/reviews/2026-08-15_semantic) | Deep semantic review of the architecture: the report, findings `A-001` to `A-003` and `M-001` to `M-014` with their resolutions, and the final audits. | 25 files, 0.3 MB |
| [`2026-08-21_codebase_refactor`](https://github.com/Yghtso/DBlusBlus/tree/reviews-archive-2026-10-21/docs/reviews/2026-08-21_codebase_refactor) | Analysis and refactoring reports behind the Phase 1 source layout (devlog 0029). | 2 files |
| [`2026-08-26_docs_separation_of_concerns`](https://github.com/Yghtso/DBlusBlus/tree/reviews-archive-2026-10-21/docs/reviews/2026-08-26_docs_separation_of_concerns) | Moving content to the document that owns it (architecture, state, development, verification), with per-section notes and the final report. | 165 files, 4 MB |

The 2026-09-26 full documentation review (devlog 0032) was never committed; its evidence stays
local to whoever ran it.
