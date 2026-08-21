• SA-002 is resolved in docs/ARCHITECTURE.md:17889.

  1. Determinism rule: For fixed successfully evaluated aggregate inputs,
     value or numeric error is independent of workers, batches,
     partitions, hash order, spilling, merge tree, and scheduling.

  2. Closed overload registry:

      Aggregate      Inputs                          Result
     ━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━
      COUNT(*)       rows                            INT64 NOT NULL
     ─────────────  ──────────────────────────────  ─────────────────────
      COUNT(expr)    any v1 scalar                   INT64 NOT NULL
     ─────────────  ──────────────────────────────  ─────────────────────
      SUM            INT32                           nullable INT64
     ─────────────  ──────────────────────────────  ─────────────────────
      SUM            INT64                           nullable INT64
     ─────────────  ──────────────────────────────  ─────────────────────
      SUM            FLOAT64                         nullable FLOAT64
     ─────────────  ──────────────────────────────  ─────────────────────
      AVG            INT32/INT64/FLOAT64             nullable FLOAT64
     ─────────────  ──────────────────────────────  ─────────────────────
      MIN/MAX        INT32/INT64/FLOAT64/VARCHAR/    nullable input type
                     DATE/TIMESTAMP

     COUNT returns zero on empty input. Other aggregates return NULL with
     no non-NULL input. Aggregate DISTINCT and FILTER remain unsupported.

  3. COUNT: Uses an exact nonnegative count or equivalent absorbing
     overflow marker. Final values through INT64_MAX succeed; larger
     counts raise NUMERIC_OVERFLOW.

  4. Integer SUM state: Exact mathematical signed integer with sufficient
     logical range. It is not a persisted TypeId and cannot wrap.

  5. Integer SUM finalization: Final exact value must fit [-2^63, 2^63-
     1]; otherwise NUMERIC_OVERFLOW. SUM(INT32) retains its INT64 result.

  6. Integer Combine: Exact addition without narrowing or host-width
     overflow. It is associative and commutative at the semantic-state
     level.

  7. FLOAT64 SUM state: Every finite binary64 input contributes its exact
     dyadic value—an integer multiple of 2^-1074. Partial combination is
     exact.

  8. FLOAT64 final rounding: One binary64 round occurs at Finalize using
     nearest, ties-to-even. Finite overflow produces signed infinity;
     subnormal results are correctly rounded.

  9. Special values: Any NaN produces canonical NaN; mixed positive/
     negative infinity produces canonical NaN; otherwise one infinity
     sign dominates all finite inputs.

  10. Signed zero: Every exact-zero SUM or AVG, including only negative-
     zero inputs, produces +0.0.

  11. Canonical cancellation vector: Every permutation of SUM(1e16,
     -1e16, 1) returns exactly 1.0.

  12. AVG state: Exact SUM state plus exact non-NULL count. Worker-local
     averages are never merged.

  13. Integer AVG: Correctly rounds the exact rational integer_sum/count
     directly to FLOAT64. It does not range-check or round a materialized
     SUM first.

  14. FLOAT64 AVG: Correctly rounds the exact rational dyadic_sum/count
     once. No rounded SUM intermediary or running average is permitted.

  15. AVG specials: NaN and infinity follow the SUM special-value rules.
     Empty/all-NULL returns NULL.

  16. MIN/MAX: Use the M-006 total order with canonical retained values.
     NaNs canonicalize; both zero signs canonicalize to +0.0. Thus MIN/
     MAX over mixed signed zeros both return +0.0.

  17. Merge laws: Exact integer/dyadic/count addition, commutative flag
     OR, and canonical MIN/MAX selection use initialized empty states as
     identities. Any merge tree finalizes identically.

  18. Vector independence: FLAT/CONSTANT/DICTIONARY representation, batch
     size, and chunk boundaries cannot alter aggregate reduction.

  19. Parallel independence: Worker-local states must implement the exact
     registry. Worker count, partition ownership, scheduling, and combine
     tree are unobservable.

  20. Spill independence: Spill may preserve exact state or replay
     qualifying values. Rounded FLOAT64 and narrowed integer partials are
     forbidden spill states.

  21. Hash versus sort: Hash and capability-enabled ordered aggregation
     use identical states and finalization. Only unspecified group output
     order may differ.

  22. Error taxonomy: COUNT/integer SUM range failure is
     NUMERIC_OVERFLOW/ArithmeticError. Exact-state memory or spill
     failure remains OutOfMemory or SpillIOError, never corruption or
     approximate fallback.

  23. COUNT early overflow: An overflow marker may be installed early,
     but the error is surfaced only at Finalize after all demanded input/
     argument evaluation.

  24. SUM intermediate range: An out-of-INT64 partial subtotal is not an
     error because later values may cancel it.

  25. Error precedence: Every group is numerically validated before any
     aggregate row is exposed. Any failing group fails the query.
     Multiple numeric failures use the lowest source-ordered semantic
     aggregate ordinal; group/hash order is irrelevant.

  26. Boundary vectors: The architecture now fixes INT64 maximum/minimum
     cancellation, COUNT at INT64_MAX and INT64_MAX+1, FLOAT64
     infinities/NaN/zeros, finite overflow, smallest subnormal, halfway
     rounding, empty/all-NULL AVG, exact integer averages, and FLOAT64
     1/3.

  27. Verification equivalence: One logical input must match across one/
     two/many workers, one-row/large vectors, different merge trees,
     forced/no spill, different spill partitions, and hash/ordered
     aggregation.

  28. Forbidden implementations: The architecture explicitly forbids
     naive FLOAT64 partial summation, host long double semantics,
     optional-width integer accumulation, partial-subtotal SUM overflow,
     COUNT wrapping, averaging averages, rounded spill states, hash-order
     reduction, premature group output, host-style constant folding, and
     approximate fallback.

  29. Sections changed: §§17.4.3, 17.10.2, 19.10, 25.8, 26.1, 29.2–29.3,
     29.6, 29.9–29.10, 32.6, 32.13, 39.1.3, 39.3.1–39.3.2, and 41.5.

  30. Signatures/TypeIds: No aggregate signature, result TypeId, or
     scalar TypeId changed.

  31. Persisted formats: No persisted format, field, width, version, or
     encoding changed. Exact aggregate states and spill representations
     remain query-local.

  32. Remaining SA-002 questions: None.
  33. Prerequisite dependencies: None discovered.
  34. New architecture questions: None introduced.
  35. Production code/tests: Untouched. Only docs/ARCHITECTURE.md
     changed; git diff --check passes. No tests were run because this was
     documentation-only architecture work.

  36. Phase boundary: Implementation Phase 2 was not entered and remains
     unauthorized.