• SA-004 is fully resolved in docs/ARCHITECTURE.md.

  1. SA-004 confirmed: The live architecture contained stale rewrite/pass provenance and audit-
     label shorthand.

  2. Categories removed: “this pass,” Pass-13, chapter-migration history, resolved-gap status
     prose, “settled” framing, and audit issue identifiers.

  3. Requirements preserved: Historical framing was replaced with direct present-tense
     requirements or canonical section references; technical content was retained.

  4. Issue labels: All live A-*, M-*, R-*, SA-*, and D-* audit-label references were removed or
     replaced with owning architecture sections.

  5. Legacy references: No references to archived architecture files, rewrite inventories,
     coverage maps, or legacy architecture remain.

  6. Rewrite/pass history: None remains. Runtime vacuum passes, optimizer rewrite phases, and
     multi-pass sorting remain because they are technical concepts.

  7. Audit/finding history: None remains.
  8. Intentionally retained matches: rewrite describes optimizer or persisted-object rewriting;
     pass describes runtime/verification algorithms; provenance is estimator/proof metadata;
     migration is a format-compatibility boundary; reconciliation is crash/storage processing;
     former, finding, and placeholder occur only in technical runtime contexts.

  9. Sections changed: Cross-reference or provenance-only edits touched §§3.3.1, 3.3.3–3.3.7;
     4.3.2, 4.11, 4.13.1–4.13.9, 4.14.5–4.14.7; 6.4; 9.12; 10.5; 11.10.4, 11.10.10, 11.13.4;
     12.12; 14.17.1; 15.5; 16.5; 17.10; 18.11.1; 20.14 and 20.20; 21.4 and 21.20; 24.8; 26.1;
     29.3; 34.3.1 and 34.14; 39.1; 41.3; Appendices A and D.

  10. Normative semantics: No normative behavior changed.
  11. SQL behavior: Unchanged.
  12. Persisted formats: Unchanged.
  13. New unresolved questions: None discovered.
  14. Prerequisite dependencies: None discovered.
  15. Resolution status: SA-004 is fully resolved.
  16. Files changed: Only docs/ARCHITECTURE.md.
  17. Verification: git diff --check passed; final targeted searches found no stale process-
     history or audit-label dependencies.

  18. Production code/tests: Untouched. No production tests were needed for this editorial-only
     change.

  19. Phase boundary: Implementation Phase 2 was not entered.