• Resolved M-006 in docs/ARCHITECTURE.md:12018.

  1. The exact v1 scalar types are BOOLEAN, INT32, INT64, FLOAT64, VARCHAR, DATE, and TIMESTAMP. NULL is a value state, not
     a TypeId. No other scalar type is implicitly available.

  2. Unsuffixed integer literals use ASCII decimal. Values fitting INT32 bind as INT32; larger values through INT64_MAX bind
     as INT64; larger values fail. Direct unary minus supports exact INT32_MIN and INT64_MIN construction without
     overflowing an intermediate positive value.

  3. FLOAT64 literals require a decimal point with digits on both sides or exponent notation. NaN/infinity keywords,
     hexadecimal forms, suffixes, and .5/1. are not literals. Overflow fails; underflow rounds to binary64 subnormal or
     signed zero.

  4. VARCHAR literals use single quotes and doubled quotes. Backslash escaping, Unicode normalization, and locale conversion
     are absent. Values are arbitrary bytes, including quoted embedded NUL.

  5. BOOLEAN literals are case-insensitive TRUE and FALSE. Numeric/string truthiness and implicit BOOLEAN conversion are
     forbidden.

  6. DATE construction uses explicit VARCHAR conversion from exact YYYY-MM-DD, years 0001..9999, with proleptic-Gregorian
     validation. DATE itself remains the settled signed INT32 day count.

  7. TIMESTAMP construction uses exact YYYY-MM-DD HH:MM:SS[.ffffff], with one to six fractional digits, no timezone, no leap
     seconds, and years 0001..9999. It remains a timezone-naive signed INT64 microsecond count.

  8. Unary overloads are closed: unary + and - for INT32, INT64, and FLOAT64; NOT for BOOLEAN. Integer minimum negation
     overflows except during direct minimum-literal construction.

  9. Binary arithmetic supports every INT32/INT64 pair and every pair involving FLOAT64 for +, -, *, and /. Integer % is
     supported; FLOAT64 %, string concatenation, and temporal arithmetic are unsupported.

  10. Mixed numeric promotion is exactly INT32 → INT64 → FLOAT64, selecting the smallest common type. It is contextual
     operator resolution, not universal coercion.

  11. Integer division truncates toward zero. Remainder satisfies a = (a/b)*b + a%b, has the dividend’s sign, and is smaller
     in magnitude than the divisor. Zero divisors fail; MIN / -1 and MIN % -1 overflow.

  12. Integer arithmetic and narrowing are checked. No signed wraparound, modulo narrowing, or host undefined behavior is
     permitted.

  13. FLOAT64 arithmetic uses IEEE-754 binary64, round-to-nearest-ties-to-even at every operator. Overflow yields infinity,
     underflow yields a correctly rounded subnormal/signed zero, and NaN results normalize canonically. Floating division by
     zero produces IEEE infinity/NaN rather than a SQL error.

  14. Comparison overloads are closed:

  - BOOLEAN: = and <>.
  - Every numeric pair: all six comparisons after numeric promotion.
  - VARCHAR, DATE, and TIMESTAMP: same-type all-six comparisons.
  - All other pairs: bind-time type error.

  15. Ordinary comparisons involving NULL produce UNKNOWN. IS NULL and IS NOT NULL return non-null BOOLEAN. Underconstrained
     NULL expressions fail binding; unique-signature contexts such as NOT NULL infer BOOLEAN.

  16. FLOAT64 SQL equality/order uses the settled canonical total semantics: signed zeros equal, all NaNs equal, NaN sorts
     after positive infinity, and finite/infinite values otherwise order numerically.

  17. B+ physical order remains broader because it includes NULL placement and BOOLEAN physical order. SQL FLOAT64 uses the
     same non-NULL total order; BOOLEAN SQL ordering remains unsupported. Broader physical bounds require semantic recheck
     where needed.

  18. VARCHAR compares unsigned bytes lexicographically, case-sensitively and locale-independently. DATE and TIMESTAMP
     compare their signed canonical day/microsecond values.

  19. NOT, AND, and OR use complete SQL three-valued truth tables. XOR and IS TRUE/FALSE/UNKNOWN are unsupported.
  20. V1 guarantees per-row left-to-right short-circuit for AND, OR, searched CASE, and expression-list IN. Skipped branches
     do not raise errors; optimizers and vectorized execution must preserve this boundary.

  21. Explicit casts are closed:

  - All identities.
  - All supported numeric conversions.
  - BOOLEAN, numeric, DATE, and TIMESTAMP to/from VARCHAR.
  - DATE ↔ TIMESTAMP.
  - No BOOLEAN↔numeric, temporal↔numeric, or other unlisted conversion.

  22. Automatic coercion permits identity, contextual NULL, and numeric widening only. Operator, CASE/IN/VALUES, assignment/
     default, aggregate, and arbitrary-expression contexts each have an exact matrix.

  23. INSERT, UPDATE, and defaults permit identity, INT32→INT64/FLOAT64, INT64→FLOAT64, and contextual NULL. Every other
     conversion requires explicit CAST.

  24. INT64→INT32 range-checks. FLOAT64→integer truncates toward zero, rejects NaN/infinity, and range-checks.
     Integer→FLOAT64 is correctly rounded and may lose precision.

  25. VARCHAR parsing consumes the complete ASCII byte string with no whitespace or junk. Integer, FLOAT64, BOOLEAN, DATE,
     and TIMESTAMP grammars and error classes are exact. FLOAT casts additionally recognize exact NaN, Infinity, +Infinity,
     and -Infinity.

  26. Scalar-to-VARCHAR formatting is canonical: uppercase booleans, minimal decimal integers, fixed temporal forms,
     explicit NaN/infinity/signed-zero spellings, and deterministic shortest round-trippable FLOAT64 output.

  27. DATE→TIMESTAMP uses midnight and checked microsecond conversion. TIMESTAMP→DATE floors toward the preceding civil
     midnight, including before the epoch. No timezone conversion occurs.

  28. Only searched CASE is supported. WHEN requires BOOLEAN; result types must match or have one common numeric promotion;
     NULL branches adopt that type. Expression-list IN/NOT IN is supported. Simple CASE, BETWEEN, LIKE, COALESCE, and NULLIF
     are unsupported. Subquery forms remain M-007.

  29. The named v1 scalar-function registry is empty. No ABS, LOWER, LENGTH, NOW, RANDOM, or library-provided function is
     implicitly supported. Aggregates remain a separate closed registry.

  30. Constant folding must reproduce runtime values and errors exactly, including integer checks, binary64 rounding,
     parsing, formatting, NULL/3VL, and control flow.

  31. Constant errors are raised eagerly only when the erroneous subtree is unavoidable under the fixed control-flow rules.
     Errors in skipped AND/OR, CASE, or IN branches are not raised.

  32. For every hashable type, semantic equality implies equal hash. FLOAT64 hashes normalize signed zeros and NaNs; VARCHAR
     hashes exact bytes.

  33. GROUP BY and DISTINCT use the same non-NULL equality but place all NULLs in one equivalence class. This NULL rule does
     not affect ordinary =.

  34. Join equality uses ordinary comparison/3VL: NULL keys do not match. Non-NULL canonical-equivalent NaNs do match, and
     hash joins must reproduce that result.

  35. M-004 remains authoritative: any NULL-containing UNIQUE key bypasses duplicate rejection; non-NULL components use the
     registry’s normalized equality and key encoding.

  36. A-002 semantic proof may use only exact, error-safe expressions from this registry. Unsupported operations,
     statistics, and estimated row counts cannot establish scalar truth or contradiction.

  37. The conceptual errors are TYPE_ERROR, INVALID_LITERAL, INVALID_CAST, NUMERIC_OVERFLOW, DIVISION_BY_ZERO, INVALID_DATE,
     and INVALID_TIMESTAMP. Runtime consequences remain governed exclusively by M-005.

  38. Parsing, formatting, comparison, arithmetic, and folding are independent of locale, timezone environment, floating
     environment, machine architecture, and libc formatting defaults.

  39. Forbidden implementations now include host signed overflow, host quotient/remainder behavior, hidden string/numeric
     coercion, locale parsing, C++-style NULL equality, raw IEEE unordered SQL NaN comparisons, inconsistent hashes,
     divergent folding, library-default FLOAT formatting, trailing-junk casts, narrowing truncation, estimate-derived
     semantics, invented overloads/casts/functions, and host-struct serialization.

  40. The canonical ownership section is docs/ARCHITECTURE.md:12028. Concise references were updated in parser literals,
     binder/operator/function/CASE/IN/ORDER BY, folding, DML/default assignment, vectorized Boolean execution, hash
     equality, aggregates, range estimation, and execution arithmetic sections.

  41. No persisted format or byte layout changed.
  42. No TypeId or TypeId numeric assignment changed.
  43. No correctness-relevant M-006 semantic question remains.
  44. No prerequisite dependency requiring a format change was discovered.
  45. No new architecture question was introduced. M-007 and M-013 remain explicitly untouched.
  46. Production code and tests were untouched. git diff --check passed; no tests were run because this was documentation-
     only.

  47. Implementation Phase 2 was not entered. The pre-existing staged M-010 changes/report were preserved untouched.