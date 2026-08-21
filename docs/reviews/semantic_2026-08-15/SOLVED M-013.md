• Resolved M-013 in docs/ARCHITECTURE.md:19429.

  1. Validation now distinctly classifies structural validity, deterministic numerical validity, and non-authoritative statistical accuracy.
  2. The exact probability tolerance is 2^-40, binary64 bits 0x3d70000000000000.
  3. Aggregate validation uses exact dyadic summation of decoded binary64 values—never host floating accumulation.
  4. Probability fields require finite [0,1]; widths/NDV/occupancy require finite nonnegative values; correlation requires finite [-1,1].
     Individual bounds have no tolerance.

  5. Persisted -0.0 is accepted and normalized to +0.0 process-locally. Canonical writers emit +0.0.
  6. null_fraction participates in exact NULL+MCV mass validation. Empty tables have canonical null_fraction=0 and a special residual of zero.
  7. MCV values must be pairwise unique under M-006 canonical equality, including signed-zero and NaN equivalence.
  8. Duplicate MCVs invalidate the entire StatsVersion; readers never merge them.
  9. MCV count is at most 64. Frequencies are finite [0,1], ordered nonincreasingly with strict scalar-order tie-breaking.
  10. Residual mass is derived exactly from 1-null_fraction-MCV_mass; differences within 2^-40 normalize to zero, otherwise negative residual
     invalidates the generation.

  11. NDV is finite, nonnegative, no greater than the exact TABLE live-row count, and bounded below by observed MCV/residual identities.
  12. Process-local HLL uses p=14, 16,384 registers, and register values 0..51. Only the bounded NDV result is persisted; no sketch format was
     added.

  13. Histograms remain one-dimensional equi-depth residual histograms, with at most 100 nondecreasing upper boundaries. Equal adjacent boundaries
     are legal and have defined grouped endpoint semantics.

  14. FLOAT64 histogram/min/max/MCV values use M-006 canonical total order: signed zeros compare equal, infinities retain their order, and NaN is
     one greatest class.

  15. HAS_MIN and HAS_MAX must appear together. MCVs and histogram boundaries must lie within min/max; arrays require min/max.
  16. Empty-table columns require zero null fraction, NDV, widths, and maximum width; no min/max, MCV, or histogram.
  17. All-NULL columns require null_fraction=1, zero NDV/widths, and no min/max, MCV, or histogram.
  18. When NULL+MCV mass exhausts probability within tolerance, normalized residual is exactly zero and the histogram must be empty.
  19. Correlation strictly accepts finite [-1,1]; undefined writer output is canonical +0.
  20. All width statistics are finite and nonnegative. Empty/no-non-NULL cases require zero; column average width cannot exceed maximum observed
     width.

  21. INDEX logical-live count must equal TABLE analyzed-live count. Physical/invisible/page/occupancy observations retain their approximate,
     concurrency-tolerant semantics.

  22. Table, scope, StatsVersion, TypeId, manifest, schema, column, and index facts must agree generation-wide.
  23. Accepted descriptors expose canonical scalar identities, positive zero, exact-derived residual mass, and deterministically proportionally
     normalized histogram masses.

  24. ANALYZE must validate its complete canonical candidate before publishing any sys_statistics row.
  25. One invalid required member invalidates the entire generation. Selection falls back to the next older complete valid generation or missing
     statistics.

  26. Caches may contain only fully validated normalized descriptors and cannot bypass validation on reload/install.
  27. A-002 remains unchanged: valid, exact-looking, or zero statistics remain costing-only and cannot prove semantic emptiness.
  28. M-006 remains the sole FLOAT64 equality, ordering, normalization, and ties-to-even rounding authority.
  29. Outcomes distinguish INVALID_STATS_GENERATION, UNSUPPORTED_STATS_VERSION, and missing statistics from malformed core catalog/page corruption.
  30. Normative vectors now cover endpoints, adjacent out-of-range values, epsilon boundaries, nonfinite values, duplicate MCVs, histogram
     ordering, degeneracies, NDV, widths, and correlation.

  31. The section explicitly forbids local epsilon choices, unordered summation, duplicate merging, illegal clamping, raw IEEE histogram
     comparisons, cross-generation salvage, and estimator-clamp legalization.

  32. Sections changed: §16.5.7; §§34.7, 34.9–34.11, 34.13, 34.14.2–34.14.6, 34.15, 34.17; §35.4; and §41.6.
  33. No persisted format, field, width, or byte layout changed.
  34. No statistics payload version changed; it remains version 1.
  35. No M-013 correctness-relevant semantic question remains.
  36. No prerequisite architecture dependency was discovered.
  37. No new architecture question was introduced.
  38. Production code and tests were untouched; only docs/ARCHITECTURE.md is modified.
  39. Implementation Phase 2 was not entered.

  Verification: git diff --check passed, and the exact epsilon’s binary64 encoding was independently checked.