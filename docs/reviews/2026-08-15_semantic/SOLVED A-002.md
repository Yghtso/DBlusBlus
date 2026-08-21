• A-002 is resolved in docs/ARCHITECTURE.md:16047.

  1. Canonical rule: numerical cardinality and truth estimates are approximate planning metadata. Only the semantic reasoning layer may originate is_provably_empty with exact, transient proof
     provenance. Estimated zero never implies semantic zero.

  2. Allowed proof sources: exact zero-row LogicalValues; safely folded constant FALSE/UNKNOWN predicates in row-rejecting contexts; exact SQL operator/type/literal contradictions; trusted
     enforced NOT NULL facts, including PRIMARY KEY nullability; actual SQL LIMIT 0; and defined operator-specific propagation.

  3. Forbidden sources: all table, column, index, sampled, cached, stale, MCV, histogram, HLL, NDV, min/max, null-fraction, row-count, join-domain, index-entry, and fallback statistics.
  4. Equality: statistical range or MCV absence may estimate zero but cannot prove absence. Exact comparisons with NULL remain exact UNKNOWN and can prove rejection only in filtering contexts.
  5. Range: statistical min/max boundaries may estimate zero or full coverage but cannot establish impossibility. Exact contradictory literal bounds remain valid proof.
  6. NULL statistics: null_fraction values of 0 or 1 are estimates only. Only enforced NOT NULL metadata supplies semantic proof.
  7. Join domains: statistical disjointness may estimate zero matches but cannot eliminate INNER/range joins or any future semi/anti join. LEFT JOIN preservation remains mandatory.
  8. Table row count: analyzed_live_row_count == 0 cannot eliminate a scan, including after concurrent or same-transaction writes.
  9. Proof propagation is now explicit for Values, Get, Filter, Project, INNER/CROSS/LEFT JOIN, grouped/global Aggregate, DISTINCT, Sort, Limit, DML inputs, and ANALYZE.
  10. Aggregates: grouped aggregation over a proven-empty child is empty; global aggregation still emits exactly one row.
  11. LIMIT: actual SQL LIMIT 0 proves emptiness. required_rows and FIRST_K_ROWS(0) remain costing objectives only.
  12. Optimizer/memo: zero estimates may affect costs, join order, access paths, and algorithm selection only. They cannot authorize logical rewrites, remove execution paths, or survive final
     validation as semantic proof.

  13. EXPLAIN/diagnostics must distinguish estimated zero from provable emptiness and expose separate estimate and proof provenance without mandating a new UI format.
  14. All nine wrong-but-plausible implementations listed in the request are now explicitly architecture violations.
  15. Changed sections: §§20.16, 20.17.8–20.17.10, 20.18–20.20; 33.4, 33.7; 34.1, 34.4–34.7, 34.14.5, 34.15, 34.17; 35.1–35.16, 35.20–35.24, 35.26–35.27; 36.15, 36.19; 37.16; 38.2, 38.6–38.7,
     38.16, 38.22–38.25; 40.7–40.8; and 41.5–41.7.

  16. No persisted format, byte layout, statistics version, or tolerance rule changed.
  17. No unresolved A-002 semantic question remains.
  18. No new architecture question was introduced.
  19. Production code and tests were untouched. PROJECT_STATE, DEVELOPMENT, VERIFICATION, and devlogs were also untouched.
  20. Implementation Phase 2 was NOT entered.