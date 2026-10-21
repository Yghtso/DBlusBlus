# 0034 — Documentation Split, mdBook and Reviews Archive

- Date: 2026-10-21
- Milestone: Documentation format follow-up (recommendations accepted by the project owner)

## Scope

Documentation and docs tooling only. No source, test or build file changed.

## Changes

- `docs/ARCHITECTURE.md` (31,489 lines) and `docs/VERIFICATION.md` (28,040 lines) became
  short index pages plus 46 chapter and appendix files under `docs/architecture/` and 30
  section files under `docs/verification/`. A one-time script moved the text and lowered
  heading levels; a check confirmed every other line was carried over unchanged. Section
  numbers are unchanged, so the 9,326 `§` references and the "ARCHITECTURE.md §x" citations
  in code still resolve.
- The docs build as an mdBook (`docs/book.toml`, `docs/SUMMARY.md`). The preprocessor
  `docs/tools/mdbook_preprocessor.py` links every `§` reference to its heading at render time
  and turns ` ```mermaid ` blocks into diagrams, which `docs/theme/mermaid-init.js` renders
  with Mermaid 10.9.1 from jsDelivr.
- `docs/tools/check_links.py` checks every internal link and anchor in the rendered book and
  in the repository Markdown. A new CI job builds the book and runs both checks. The first
  run found three broken links, all fixed.
- Two Mermaid diagrams: buffer frame ownership states (§7.6.2) and the heap slot lifecycle
  (§4.13.3). The tables stay canonical.
- `docs/reviews/` (324 files, about 20 MB) moved to the tag `reviews-archive-2026-10-21`,
  which points at the main commit that still contains it. `docs/reviews/README.md` indexes it.
- `AGENTS.md` went from 593 to about 230 lines. Every rule is kept, but stated once, and it
  now describes the chapter layout and the book.
- Mechanical tightening of the chapter files: 710 runs of one-line paragraphs merged (at most
  five sentences each) and 288 one-line ` ```text ` blocks turned into inline code. A
  token-level check confirmed the word sequence of every file is unchanged. About 2,900
  fewer lines.

## Not done

- The open question from the docs report, whether to cut architecture scope, was not
  decided and nothing was cut.
- The search index is about 18 MB; mdBook warns about it but it works.
