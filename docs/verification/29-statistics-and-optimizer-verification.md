# Statistics and Optimizer Verification

## Statistics Tests

Test ANALYZE on controlled distributions:

```text
uniform integers
highly skewed MCV
many NULLs
all same value
all distinct
two-mode distribution
monotonic values
VARCHAR prefixes
FLOAT64 edge values
```

Verify:

```text
row counts
NDV accuracy bounds
MCV detection
histogram boundaries
null fraction
width estimates
```

Use exact reference counts for small inputs and fixed-seed generators for larger inputs.
Run each applicable distribution through in-memory candidate construction, canonical
serialization/decoding, and normalized descriptor materialization. Approximation quality
may vary within the architecture's contract, but descriptor validity and normalization
must not depend on input order, hash iteration, host rounding mode, or locale.

When §34.8's bounded small-table exact mode applies, compare every non-NULL frequency and
NDV with an exact map. Repeat immediately below/above the configured threshold and with an
insufficient maintenance-memory budget to verify that changing collection mode changes
approximation strategy, not descriptor validity or SQL semantics.

## ANALYZE Performance Benchmark

Apply the shared Chapter-42 benchmark evidence policy to explicit `PhysicalAnalyze`.
Reuse Statistics Tests, Statistics Algorithm Tests, Statistics Publication and Versioning
Tests, Statistics Persistence and Validation Tests, and V34-A/F/H/I for correctness;
speed or compactness cannot substitute for a complete valid candidate and successful
statement. The data-collection path is Chapter 34's vectorized *full heap scan*, not
page/block sampling, automatic scheduling, or persisted incremental sketches. Internal
bounded reservoir sampling and permitted physical index-statistics sampling do not
replace that heap scan. One coordinator owns the statement; no parallel ANALYZE is
required. This benchmark does not calibrate base access or estimator error distributions.

Predeclare deterministic table fixtures with stable TableId, SchemaVer, ColumnIds/types,
index manifest, generated values/seed, visible row and physical heap-page counts, row/
VARCHAR widths, NULL/distinctness/skew distributions, BufferPool capacity, maintenance
budget, and expected valid statistics generation. Include fixed-width, nullable, and
VARCHAR columns, a material row-width difference, multiple pages and nonzero visible
rows. For a row-scale series vary rows/pages while holding analyzed schema and value
distributions materially comparable; for a column-scale series vary explicit analyzed
base ColumnIds/types while holding the visible row population comparable. Record exact
fixture parameters rather than treating “large” as a scale. Use a stable table/snapshot
without concurrent DML for the primary comparison; Chapter-34 concurrent-DML fixtures
remain the visibility oracle. Record physical versions encountered separately from
visible rows because dead/invisible versions may consume scan work without contributing
to SQL-visible statistics.

The primary measured interval begins when the resolved `PhysicalAnalyze` statement
starts its required collection and ends at successful ANALYZE *statement* completion:
full scan, bounded collection/finalization, index-statistics work where required,
candidate validation, payload encoding, and transactional `sys_statistics` row writes
are included. Require the complete valid transaction-local descriptor and all required
rows at that boundary (§§31.12.1, 34.3); terminal transaction COMMIT/C4/C5 global cache
publication is a separate outcome, not silently included in this statement rows/sec
interval. If scan, finalization, encoding, or publication component times are reported,
label their own owner-observed boundaries separately. Aborted, canceled, corrupt,
resource-failed, or partial candidates are not successful performance samples; retain
their canonical outcomes. A declared failure-stress diagnostic cannot replace this run.

| Required metric | Denominator/scope and canonical source | Positive control / prerequisite |
|---|---|---|
| Rows/second | Snapshot-visible heap rows actually accepted into Chapter-34 column/table collection, divided by primary statement elapsed seconds; direct scan/collector count reconciled with independently expected visible rows, not output estimates or sampled values | Nonzero visible rows, multiple chunks/pages, complete valid ANALYZE |
| Columns analyzed | Distinct base ColumnIds whose required column-statistics state was actually collected and appears in the complete TABLE manifest; one ColumnId counts once despite HLL, MCV, and histogram substructures | Column-scale series and per-ColumnId collection/member evidence, not schema width |
| Peak maintenance memory | Maximum simultaneous live/charged ANALYZE-owned bytes during the interval, from `PhysicalAnalyze` QueryMemoryManager/maintenance-region owner ledger (§§24.4, 31.12.1; V34-H/V24-B/D) | Growing collection/candidate fixture, valid cleanup/transfer; not RSS or sum of separate component peaks |
| HLL memory | Process-local HLL owner live state/capacity bytes and simultaneous column states, separately from persisted NDV | High-cardinality non-NULL HLL-active column and nonzero owner work/state |
| MCV memory | Heavy-hitter owner live state/capacity and retained value bytes, separately from final MCV payload | Skewed MCV-active column with observed owner state |
| Sample memory | Reservoir/sample owner capacity, occupancy, element and owned varlen backing bytes | Sample-active column with observed nonzero state |
| Statistics payload bytes | Sum of exact `total_length` for valid reassembled §34.14 TABLE/COLUMN/INDEX `StatisticsPayloadV1` byte sequences in this StatsVersion, with per-scope subtotals | At least one nonempty complete encoded generation passing chunk/CRC/manifest/semantic validation |
| Full-scan I/O | Required heap pages logically visited; same-scan BufferPool hits/misses and completed physical heap reads/pages or bytes from Chapter-7/40 storage owners, reported separately | Independently known multi-page extent, observed full required page coverage and owner I/O events |

The rows/sec numerator deliberately uses Chapter 34's SQL-visible analyzed live-row
population: the full scan supplies every row visible to the one ANALYZE snapshot exactly
once (V34-036), and those rows produce `analyzed_live_row_count` and column statistics.
Physical tuple versions visited, dead-version pressure, and heap pages visited are
separate diagnostic work counts, not substitute rows/sec numerators. The direct collector
event must agree with the independent snapshot fixture; the final TABLE row-count field
alone is not a work counter. A required column enters collection and contributes a
valid COLUMN candidate before it counts; table-level fields, hidden catalog fields,
index-key components, and fragments do not add base columns. One column with three
substructures still counts once. No rows/sec, memory, or payload pass threshold is set.

Capture one live maintenance-memory event history so its peak is a simultaneous maximum,
not HLL peak plus MCV peak plus sample peak measured at different instants. Identify
scan chunks/vectors, HLL/MCV/reservoir state, owned VARCHAR values, candidate/encoding
buffers, and maintenance-owned I/O buffers under their canonical QueryMemoryManager or
collection-region charges; V34-H/V24-B/D check coverage, transfer, and release. Record
HLL, MCV, and sample components separately and reconcile their live charges with the
whole ledger; other live ANALYZE state may make the total larger. HLL's configured
register formula may check but not replace observed owner-backed bytes. A representative
fixture must activate HLL, MCV, and reservoir states simultaneously where Chapter 34
permits, without requiring every column to use each structure. Retained VARCHAR samples/
MCVs need stable owned bytes, not a borrowed heap-page lifetime (V34-H/V23-G). Exclude
the separately owned BufferPool frame pool, unrelated queries and RSS. At valid transfer
to immutable descriptor/catalog/cache ownership, preserve continuous coverage without
double-charging the old maintenance owner indefinitely; transient cleanup must release.

Measure §34.14 encoded payloads *before* their at-most-4096-byte catalog-row
fragmentation: report TABLE, COLUMN, applicable INDEX, and whole-generation length
subtotals from canonical reassembled `total_length` values. A nonempty payload positive
control and varying column/type/MCV/histogram/index fixtures expose useful payload-size
changes. Fragment count, catalog tuple/page bytes, publication writes, and WAL bytes may
be supplementary, separately labeled metrics; file growth or final descriptor object
size is not payload bytes. Require the existing round-trip, CRC, manifest-completeness,
and numerical-validity oracles before accepting a payload measurement. HLL registers and
reservoir state are process-local, not extra persisted payload fields.

For full-scan I/O, compare the independently known required heap-page extent with direct
scan-page visits. Record table working-set pages versus actual BufferPool frame capacity,
preconditioning, same-interval owner hits/misses, and *completed physical heap reads*
separately from logical visits. Use one declared baseline residency regime across a
comparison; an additional mostly resident or larger-than-BufferPool case may diagnose
I/O. If deliberate file-cache deconditioning is used, record its method and call it
less-warm unless stronger evidence exists, as in Storage Benchmarks. No perfectly cold OS
cache or mandatory platform cache eviction is assumed. A sampled subset of heap pages
does not activate full-scan collection. Index full-walk/bounded-sample reads and
catalog publication writes/WAL are separately scoped from heap full-scan I/O; row count
times row width and heap-page count alone are not physical-read observations. A column-
scale change may increase decoding/memory while leaving heap-page extent unchanged.
Missing required physical-read observations follow the shared NOT VERIFIED policy, not
an inferred zero. HLL quality, MCV/histogram correctness, snapshot visibility, index
statistics, persistence, and publication remain with their Chapter-34/V34 owners.

---

## Statistics Algorithm Tests

### HLL NDV collection

Exercise the process-local HLL state defined by [`ARCHITECTURE.md`](../ARCHITECTURE.md)
§§34.9 and 34.14.6.5 with empty input, one-value and very small domains, medium and large
known cardinalities, heavy duplicate repetition, and, when the collector exposes a merge
operation, disjoint partitions merged in varied orders. Assert:

```text
NULL never enters the sketch
canonical-equal values hash as one value
precision/register count and register domains are exact
the bounded writer result lies between observed lower bounds and the exact non-NULL count
nonfinite, negative, or malformed candidate state fails before persistence
```

Keep those structural/persisted checks (including V34-038 and V34-050) distinct from the
following §34.9 pre-bound set-invariance and post-bound distributional-quality oracles.
Together they discharge §41.6's `HLL error distribution` obligation. The architecture fixes `p = 14`,
`m = 2^p = 16,384`; precision is not a runtime quality setting. An internal test seam may
observe the completed process-local pre-bound estimate, the actual §34.14.6.5 bounded
candidate, and proof that HLL collection ran; no public SQL option or persisted field is
required. When merge is supported, partitioned collection is subject to the same applicable
§34.9 output rules; merge/parallel collection is not required by the defined architecture.

**Exact pre-bound set-invariance.** Reuse V34-041's applicable HLL scalar families
(integer, BOOLEAN, DATE, TIMESTAMP, VARCHAR, FLOAT64) and their Chapter-17 canonical
equality/hash fixtures, including FLOAT64 canonical NaNs and equivalent signed zeros;
do not assume unsupported types use HLL. Independently form one non-NULL canonical
distinct-value set per fixture without reading HLL state, estimated NDV, or statistics
descriptors. Present the same set as all singletons, all pairs, mixed singleton/repeated,
and strongly skewed duplicates; use ascending, descending, a fixed nontrivial permutation,
clustered and interleaved duplicates, and at least two chunk partitions. Exercise the
HLL component even where configured §34.8 exact mode would otherwise run. For every
successful pair of collections of the same set, compare the finite canonical binary64
pre-bound estimates for exact numerical equality (zero is `+0`), without a tolerance.
Register bytes, temporary state, and traces are not equality oracles. Include canonical
hash-collision fixtures where available; distinct SQL values need not have distinct hashes.
The singleton/pair pre-bound estimates MUST agree, but their bounded candidates need not:
if their shared pre-bound estimate exceeds exact `N`, the singleton writer cap is `N`
and the paired cap is `2N`. Exercise the construction bounds with a synthetic valid
pre-bound `N+1` and an INT64 set with `N > 64`: the singleton candidate is `N`
and the paired candidate is `N+1`. Do not assert post-bound equality. Use feasible fixtures and
adequate resources to require successful output under presentation-only changes; an
invented singleton/order/duplicate/chunk error is not permitted. For a separately
injected genuine resource, cancellation, or lower-layer failure, preserve its existing
cause and compare estimates only when both exist; failure is not an HLL quality PASS.

**Post-bound distributional quality.** For each `N` in the fixed grid
`{4m, 16m, 64m} = {65,536, 262,144, 1,048,576}`, use 1,600 independently indexed
distinct-set trials `i = 0..1599`. For each `(N,i)`, sample one `N`-element subset `S`
of the complete signed INT64 domain; reuse that `S` across all five policies below.
This produces 1,600 trials for each of 15 `(N,P)` cells. All policies are fixed
deterministic mappings of `S`, not chosen after seeing an estimate:

```text
P1  ascending signed values, two adjacent copies per value; R = 2N
P2  descending signed values, two adjacent copies per value; R = 2N
P3  Fisher-Yates permutation of the 2N paired occurrences; R = 2N
P4  ascending clustered values, min(S) 32 times and each other value twice; R = 2N+30
P5  one ascending and one descending pass, then 30 extra min(S) copies
    inserted after base positions floor(k*2N/31), k=1..30; R = 2N+30
```

Use a test-only SHAKE256 byte stream for replay: the `S` stream input is the exact
byte concatenation `UTF8("DBlusBlus-HLL-quality-v1/S") || U64BE(N) || U64BE(i)`;
the P3 stream input is `UTF8("DBlusBlus-HLL-quality-v1/P3") || U64BE(N)`
followed by the ascending signed `S` values encoded individually as canonical
64-bit two's-complement big-endian words. P3's stream depends on `S`, not on
the trial index, so it defines one complete admissible policy. Consume
successive stream bytes as big-endian uint64 words.
Fisher-Yates swaps slot `k` with a bounded draw in `0..k` for
`k = 2N-1 .. 1`. For `S`,
use Floyd's without-replacement selection over unsigned words: for integer
`j = 2^64-N .. 2^64-1`, draw `t` uniformly in `0..j`; insert `j` if `t` is
already selected, otherwise insert `t`. Map each selected unsigned word `u`
to signed INT64 value `u` when `u < 2^63`, else `u-2^64`. For a draw in
`0..j`, reject 64-bit words below `2^64 mod (j+1)` and reduce an accepted
word modulo `j+1`; when `j=2^64-1`, use the word directly. This makes the
subset algorithm uniform under independent uniform source words; the fixed
pseudorandom stream is reproducible sampling evidence, not a claim that one
fixed run proves the full Architecture population. Record stream inputs and
the selected-set digest for replay; no production RNG or hash seed is added.

For every scheduled trial, independently confirm `|S|=N`, the generated
presentation's exact canonical distinct set is `S`, its exact non-NULL row
count is the stated `R`, and the ordinary HLL collection path actually ran.
Exact mode or another NDV shortcut is not HLL evidence. Take `N_hat` from the
real finite binary64 post-construction-bounds candidate, not the pre-bound
estimate, registers, persisted/stale descriptor, or planner estimate. A
structurally invalid candidate is a separate §34.14.6.5 failure, not a finite
quality sample. Using the mathematical value of `N_hat`, compute
`e = (N_hat-N)/N` and `X = e^2`; `N`, not `R`, is exact NDV.

For these nonempty fixtures §34.14.6.5 gives `1 <= N_hat <= R` (the observed
MCV lower bound may be stronger). Hence `a = 1/N-1 <= e <= b = R/N-1`,
`0 <= X <= M^2` with `M = max(abs(a), abs(b))`. Thus P1–P3 have `b=1`,
`M=1`, while P4–P5 have `b=1+30/N`, `M=1+30/N`; in every cell the error
width is `w = b-a = (R-1)/N`. No normal/HLL error-shape assumption is used.
Architecture's population limits are `Q = 4/m = 0.000244140625` for `E_S[X]`
and `L = 1/sqrt(m) = 0.0078125` for `abs(E_S[e])`. They are ceilings, not per-sketch bands;
one `abs(e) > 2/sqrt(m)` does not by itself fail quality.

Use a predeclared indifference-zone conformance test. The Architecture GOOD
region is `E_S[X] <= Q` and `abs(E_S[e]) <= L`; the gross alternatives are
`E_S[X] >= G2 = 0.04` (RMS at least `0.20`), `E_S[e] >= G1 = 0.10`, and
`E_S[e] <= -G1`. `G1/G2` are Verification power targets, **not** replacement
Architecture limits. Predeclare family-wise false-rejection budget
`alpha = 0.01` across the 15 cells and three one-sided checks per cell
(`X`, positive bias, negative bias): `delta = alpha/45 = 1/4500` per check.
Predeclare false acceptance `beta = 0.01` per cell for each gross alternative,
under independent uniform-`S` sampling. Set `n = 1600` for every cell,
`z = ln(4500)`, `B_* = (1+30/65536)^2`, `W_* = 2+29/65536`, and the fixed
thresholds below. `B_*` and `W_*` dominate every selected policy's `M^2`
and `w`, respectively.

| Policies | Exact error support `[a_P,b_P]` | `X` support | Trials | M2 threshold | Signed-bias thresholds |
|---|---|---|---:|---:|---:|
| P1–P3 | `[1/N-1, 1]` | `[0,1]` | 1600 | `T2=0.0033` | `-T1`, `+T1` |
| P4–P5 | `[1/N-1, 1+30/N]` | `[0,(1+30/N)^2]` | 1600 | `T2=0.0033` | `-T1`, `+T1` |

For both rows, `T1 = L + sqrt(2*Q*z/n) + 2*W_*z/(3*n)`
(`approximately 0.01642612485866581`). The shared calibration is
`delta=1/4500`, `beta=0.01`; bounded-mean Chernoff/KL bounds calibrate M2
and the gross-bias tails, while one-sided Bernstein with the Architecture
second-moment bound calibrates the GOOD-side signed-bias checks. Define
`d(u||v) = u*ln(u/v) + (1-u)*ln((1-u)/(1-v))` for `0<u,v<1`.
For each cell calculate `xbar = sum(X_i)/n` and `ebar = sum(e_i)/n`.
Evaluate these real-number comparisons with validated precision, refining
interval bounds until each `<=` decision is resolved; host floating-rounding
tolerance is not an oracle.

```text
PASS cell iff xbar <= T2 AND -T1 <= ebar <= T1
PASS campaign iff all 15 cells pass, all scheduled trials and pre-bound
    invariance checks complete, and all structural/path/nonvacuity checks pass
```

For `0 <= X <= M^2 <= B_*`, bounded-mean Chernoff domination by a Bernoulli
variable gives the GOOD-side upper-tail bound
`Pr(xbar > T2) <= exp(-n*d(T2/B_* || Q/B_*)) < 0.000143 < delta` when
`E_S[X] <= Q`: the exponent is greater than `8.8587`, exceeding
`ln(4500)`. Under `E_S[X] >= G2`, its lower-tail counterpart gives
`Pr(xbar <= T2) <= exp(-n*d(T2/B_* || G2/B_*)) < 6e-21 < beta`:
the exponent exceeds `46.61`. These bounds hold for each selected policy;
using its smaller exact `M^2` can only tighten them.

For the signed-bias GOOD side, the *complete* Architecture contract supplies
`Var(e) <= E_S[e^2] <= Q`; the range is at most `W_*`. One-sided Bernstein
therefore gives `Pr(ebar > T1) <= exp(-z) = delta` whenever
`E_S[e] <= L`, and `Pr(-ebar > T1) <= delta` whenever `E_S[e] >= -L`.
The 45-check union bound gives family-wise false rejection at most `alpha`,
without requiring independent results across policies. The bias checks do
not assume the Architecture RMS bound on the GROSS-BAD side: for each policy
set `w_P=b_P-a_P`. Bounded-mean lower-tail Chernoff applied to
`(e-a_P)/w_P` gives, when `E_S[e] >= G1`,
`Pr(ebar <= T1) <= exp(-n*d((T1-a_P)/w_P || (G1-a_P)/w_P))`.
Applied to `(b_P-e)/w_P`, it gives, when `E_S[e] <= -G1`,
`Pr(ebar >= -T1) <= exp(-n*d((T1+b_P)/w_P || (G1+b_P)/w_P))`.
Across all six `(N,policy-support)` classes, the positive and negative
exponents exceed `5.6164` and `5.6168`, respectively, so their false
acceptance probabilities are below `0.00364 < beta` per cell. Independently,
`abs(E_S[e]) >= 0.10` implies `E_S[X] >= 0.01`; the M2 check then has a
lower-tail exponent exceeding `4.8979`, giving false acceptance below
`0.00747 < beta`. This is additional composite power, not a substitute for
the signed-bias checks.

Under the ideal independent uniform-set model, the procedure thus protects
Architecture-conforming populations at family-wise `alpha` and has at least
`1-beta` per-cell power against each stated gross alternative. A fixed
SHAKE256 run is replayable evidence, not literally independent random draws.
PASS does **not** prove the exact population inequalities, all admissible
presentation policies, individual-estimate correctness, semantic proof, or a
production performance target. The fixed policies provide finite evidence
for §34.9's universal rule, not its replacement. Populations between an
Architecture limit and the corresponding gross alternative may pass or fail;
the Architecture alone defines conformance in that indifference zone.

Do not drop, resample, or count as zero-error any missing trial, invalid
candidate, path-not-exercised trial, or genuine resource failure. Classify
such a campaign as non-PASS with its structural/setup/resource cause; do not
reinterpret a real `OutOfMemory` or cancellation as HLL error. Record on
non-PASS: `p/m`, `N`, `P`, `i`, exact `S` and P3 stream inputs and set
digest, exact `N/R`, HLL-path evidence, pre-bound estimate when available,
`N_hat`, `e`, `X`, cell sums/means, `a/b/M/w`, `Q/L/G1/G2`, `n/delta/beta`,
`T2/T1`, the applicable concentration bound and violated decision, and any
owner failure. Self-test the decision logic, separately from HLL collection,
with synthetic trial vectors: constant `e=+0.10`, constant `e=-0.10`,
alternating `e=+0.20,-0.20`, constant `e=+0.25`, and alternating
`e=+0.25,-0.25` must all be non-PASS; all-zero errors must be on the
quality-statistic acceptance side, subject to the normal nonvacuity gates.
Feed exact-arithmetic sample statistics at `T2` and `+T1/-T1` and
immediately on each side: equality is accepted when the other checks pass,
and crossing a threshold is non-PASS. Also exercise synthetic pre-bound
singleton/order/profile/chunk mismatches; an isolated tail value; exact-mode
substitution; missing
trials; changed/unrecorded seeds or post-hoc thresholds; nonfinite candidate;
and circular HLL-derived truth. Invariance mismatches, gross synthetic
errors, and invalid setup must be non-PASS in their respective category;
an isolated tail is not an automatic failure, and a row-count-cap-only
singleton/pair candidate difference is permitted.

### Heavy hitters and MCVs

Feed the bounded heavy-hitter collector fixtures with one dominant value, several dominant
values, values immediately around the top-entry cutoff, uniform data, many NULLs, and a
long duplicate-heavy tail. Compare with exact reference frequencies and assert:

- representable true heavy hitters are retained according to the collector's bounded-error
  contract;
- candidate count stays within the §34.10 bound and no unbounded exact map is required;
- NULL is excluded from MCV identity while each persisted frequency remains relative to
  all visible rows;
- canonical-equal FLOAT64 values and exact VARCHAR bytes use the statistics equality mode;
- canonical ordering, uniqueness, frequency bounds, and tie ordering satisfy §34.14.6.3;
- MCV mass is removed exactly once from the residual population.

Vary input and merge order to exercise bounded-error behavior. Approximate candidate sets
need not be identical unless the collector contract requires it; every resulting accepted
set must nevertheless receive the same deterministic canonical validation, ordering, and
normalization.

### Reservoir and histogram construction

Use fixed reservoir choices or an injectable deterministic sampler so the same sampled
state can be replayed. Cover empty/tiny input, all-equal values, many duplicates, monotonic
input, bimodal/skewed data, exact bucket-count boundaries, and samples larger than the
configured reservoir. Test the structure directly rather than only through downstream
selectivity.

After MCV removal, assert that histogram boundaries are nondecreasing under the statistics
scalar order, equal adjacent boundaries retain their defined grouped-boundary meaning,
bin count remains bounded, each mass is valid, and normalized residual-bin mass covers the
residual population within §34.14.6's exact tolerance. Decreasing boundaries, MCV/boundary
identity overlap, impossible mass, and illegal degenerate forms invalidate the complete
candidate.

### Value order, mass, and width

Use controlled rows with calculable NULL, MCV, residual, and width components. Verify NDV
excludes NULL, NULL fraction is separate, MCV and histogram mass do not overlap, and
probability components satisfy the exact dyadic construction and normalization rules in
§34.14.6. Width tests include empty/all-NULL columns, fixed-width values, and VARCHAR
length distributions, including maximum-observed-width boundaries.

Statistics scalar-order fixtures include:

```text
VARCHAR: empty bytes, embedded zero bytes, strict prefixes, high unsigned bytes,
         and values around any persisted-scalar length boundary
FLOAT64: negative finite values, -0.0/+0.0, infinities, canonical-equivalent NaNs,
         and adjacent total-order values
```

Assert binary VARCHAR ordering and the canonical FLOAT64 total order. Raw object bytes,
locale collation, and host unordered-NaN comparison are not valid oracles.

---

## Statistics Publication and Versioning Tests

### ANALYZE snapshot and candidate ownership

Run ANALYZE with deterministic barriers while concurrent transactions insert, update,
delete, commit, and abort. Capture the statement's effective snapshot and independently
compute its visible rows. Assert that SQL-visible row/column statistics include exactly
that snapshot, later visibility changes do not mutate the in-progress candidate, and
physical heap/index-pressure observations remain separate approximate metadata. Reuse the
MVCC fixtures above for visibility semantics rather than defining another snapshot model.

For each statement attempt, assert one exact `StatsVersion = (TxnId, CommandId)`, one owner
transaction, and one version on every TABLE/COLUMN/INDEX row and payload. Repeated ANALYZE
commands in one transaction receive distinct monotonic CommandIds. Cross-version chunks,
invalid identifier domains, and row/payload version disagreement reject the generation.

### Manifest completeness and terminal publication

Build a complete version, then independently remove, duplicate, reorder, corrupt, or
substitute each required chunk and each TABLE-manifest member. Also add an unlisted or
wrong-object COLUMN/INDEX member. The loader must accept only one complete, internally
consistent generation; it never combines members from different StatsVersions or salvages
valid scopes from a failed generation.

Place barriers before statement completion, before terminal COMMITTED publication, and at
the C5 cache side effect. Verify:

```text
the owner may use its complete transaction-local descriptor after the CommandId boundary
other transactions keep using the prior committed descriptor before terminal COMMITTED
ABORT/cancellation and incomplete candidates never publish globally
a complete committed generation becomes eligible atomically
existing planners retain their immutable descriptor
```

After durable COMMIT, inject cache allocation/installation and delayed-callback failure.
The transaction remains COMMITTED, the persistent complete generation remains usable, and
the cache establishes the architecture-approved older/missing safe fallback without
publishing a partial descriptor. Cross-reference the §41.3 post-durable COMMIT procedures;
this section does not restate C0–C6.

### Generation selection and status reclamation

Provide an older complete generation and newer variants that are complete, incomplete,
aborted, unsupported, or corrupt. Load through ordinary catalog MVCC and assert selection
of the greatest visible committed complete valid StatsVersion, otherwise the older valid
generation or explicit missing-statistics fallback. Concurrent publication must not let a
delayed older install replace a newer applicable descriptor.

Freeze/reclaim the outer catalog tuple's creator status, retire the StatsVersion creator's
status history, close/reopen, and reload. Assert that the opaque numeric StatsVersion is
unchanged, directly comparable, and usable without transaction-status lookup, retained
status pages, or terminal-cache reconstruction. Outer catalog MVCC visibility and payload
identity remain distinct.

### One stable statistics snapshot per optimization

Use optimizer hooks to publish generation B after one optimization has selected generation
A but before it finishes costing. Every table/column/index lookup in that invocation must
retain A's immutable applicable descriptors; no mixed A/B generation may enter one plan.
A later optimization may select B. Repeat with a failed B publication and with a
transaction-local descriptor visible only to its owner.

---

## Statistics Persistence and Validation Tests

Construct catalog rows and chunked payloads directly so each validation layer can be
isolated. Corrupt one aspect at a time:

| Area | Cases | Required result |
|---|---|---|
| Outer/chunk framing | Missing, duplicate, noncontiguous, reordered, trailing, or size-inconsistent chunks | Reject the generation after safe row/chunk reconstruction; malformed core catalog framing retains its owning corruption outcome |
| Header/identity | Magic, flags/reserved fields, scope, payload version, length arithmetic, CRC32C, TableId/ColumnId/IndexId, TypeId, StatsVersion, or schema/object fingerprint mismatch | Classify invalid versus well-framed unsupported statistics exactly as §§34.14.5–34.14.6 require |
| Scalar payload | Truncated/malformed codec, NULL where forbidden, wrong TypeId, noncanonical persisted scalar, or invalid VARCHAR/FLOAT64 carrier | Reject the whole StatsVersion; never parse best-effort |
| Probabilities/widths | NaN, infinity, negative or above-one probability, negative/nonfinite width, illegal correlation, or impossible cross-field relationship | Reject rather than clamp an individually invalid field |
| MCV/histogram | Duplicate canonical identity, invalid tie order, decreasing boundary, MCV/boundary overlap, excessive counts, bad aggregate mass, or illegal degenerate form | Reject the entire generation; never merge/drop individual entries |
| NDV/index consistency | NDV outside §34.14.6.5 bounds, invalid min/max relationship, wrong logical-live index count, or cross-generation disagreement | Reject generation-atomically |

Directly execute every normative boundary vector in §34.14.6.10, including `+0`, `-0`,
one, adjacent out-of-domain values, exact sums at and around `2^-40`, duplicate signed-zero
and canonical-NaN MCV identities, equal/decreasing histogram boundaries, empty/all-NULL/
one-value columns, exhausted residual mass, NDV limits, and correlation endpoints. Use
exact dyadic/rational fixture construction so acceptance does not depend on host floating
evaluation.

For each accepted payload, assert one deterministic process-local descriptor: numerical
zeros normalize to `+0`, scalar identities and MCV order are canonical, residual and bin
masses use the architecture's one-rounding normalization, and serialize/decode yields the
same semantic descriptor. Semantically identical permitted encodings normalize equally;
rejected corruption is never treated as another normalization path.

Use two tables with identical visible rows/selectivity but different dead versions,
physical B+ entries, invisible entries, leaf pressure, and heap correlation. Logical row,
NDV, and selectivity estimates remain equivalent while physical scan/index-pressure fields
and costs may differ. Zero physical/live-entry statistics never suppress runtime access.

---

## Selectivity Estimation Tests

For synthetic data with known distributions, compare:

```text
estimated selectivity
actual selectivity
q-error
```

for:

```text
=
<
<=
>
>=
equivalent lower/upper bound conjunctions
IN
IS NULL
IS NOT NULL
AND
OR
NOT
same-column ranges
```

Include values:

```text
inside MCV
outside MCV
outside min/max
near histogram boundaries
```

For every primitive predicate, inspect the complete finite
`PredicateTruthEstimate {TRUE, FALSE, UNKNOWN}` and assert normalization and legal bounds,
not only filter row count. Directly execute the architecture-owned cases in §§35.6 and
35.10–35.16:

- equality MCV hit/residual/outside-range behavior;
- range boundary inclusion and residual histogram interpolation;
- `IS NULL` and `IS NOT NULL` with and without enforced NOT NULL;
- IN lists with no NULL, one/multiple NULLs, duplicates, and no matching value;
- NOT, AND, and OR over controlled truth triples, including independence fallback;
- same-column equality/range/IN/NULL intersections and exact contradictions.

Direct `BETWEEN` estimator dispatch is not a scalar-registry capability under §35.10;
assert it cannot bypass front-end support checks. The estimator exercises the equivalent
registered lower/upper-bound conjunction only when supplied as a valid normalized logical
predicate.

Same-column fixtures cover overlapping and nested ranges, touching inclusive/exclusive
boundaries, contradictory bounds, and equality inside/outside a range. Assert the
normalized constraint-set path is used instead of multiplying independent estimates.
Statistical min/max can influence its numerical estimate but cannot create the exact
contradiction.

Use exact or deliberately boundary-straddling scalar inputs for probability normalization,
histogram interpolation, and clamping. Keep estimator finalization distinct from persisted
statistics validation: downstream clamping must not legalize malformed statistics.

Measure q-error against actual/reference counts for quality regressions, recording
confidence and provenance. A high q-error is a performance-quality failure under the
chosen regression threshold, not permission to change SQL semantics; conversely, a small
or zero estimate used as semantic proof is a correctness failure regardless of q-error.

---

## Semantic Emptiness Tests

Carry each fixture through estimator output, logical/physical alternative construction,
memo insertion/dominance, final optimizer validation, and differential execution. At every
layer inspect numerical estimates and semantic-proof metadata separately. Unless the exact
§35.2 whitelist and §20.17.10 propagation rule apply, assert:

```text
estimated_rows may be 0
is_provably_empty remains false
normal access/join alternatives remain enumerable
no empty/no-op replacement is accepted
execution still runs and matches the reference result
```

The semantic-emptiness matrix includes:

| Fixture | Required proof/result |
|---|---|
| Rounding, sampling, histogram endpoint, absent MCV, HLL/NDV approximation, independence, clamping, or missing fallback yields numerical zero | No semantic proof; executable alternatives remain |
| Analyze an empty table, then commit a visible matching insert while retaining the stale zero descriptor | Estimate may remain zero; scan/index alternatives remain and execution returns the inserted row |
| Stale min/max, histogram, or MCV suggests an equality/range value is outside the analyzed domain | No proof; a later visible matching row is returned |
| Statistically disjoint join domains later gain a visible overlapping key | No proof; join remains executable and returns the match |
| `null_fraction` is 0 or 1 without an enforced constraint | No proof for `IS NULL`/`IS NOT NULL`; schema constraint and statistics remain distinct |
| Enforced NOT NULL contradicts `IS NULL` | Approved trusted-constraint proof in a row-rejecting context |
| Complete constant FALSE or UNKNOWN predicate, exact contradictory typed literals, or zero-row `LogicalValues` | Approved exact proof only in the contexts permitted by §§20.17.10 and 35.2 |
| LEFT JOIN has a proven-empty right side or an ON predicate proven never TRUE | Join is not empty; preserve every left row and null-extend it |
| LEFT JOIN preserved side is proven empty | Propagated empty proof is accepted |
| Grouped aggregate over a proven-empty child | Empty proof propagates |
| Global aggregate over the same child | Not empty; execution produces the architecture-defined one row |
| Actual SQL `LIMIT 0` | Approved `SQL_LIMIT_ZERO` proof |
| `FIRST_K_ROWS(0)` or another costing objective is zero | No semantic proof and no executor row cap is fabricated |
| Table/index live or physical-entry estimate is zero | Runtime SeqScan/usable IndexScan and heap visibility work are not suppressed |

For every positive proof case, mutate the provenance to statistics, confidence, cost, or
an unapproved metadata source and assert logical/physical/final validation rejection. For
every non-proof zero estimate, perform optimized-versus-reference execution with visible
rows. This is the canonical estimated-zero regression and must also pass through the memo
tests below.

---

## Join Estimation Tests

Synthetic joins:

```text
unique-to-many
many-to-many uniform
hot-key skew
disjoint domains
partially overlapping domains
NULL-heavy keys
duplicate-heavy MCVs
```

Compare baseline NDV estimator against MCV-aware estimator.

Store regression expectations for controlled high-q-error cases.

For each distribution, compute reference pair counts and complete NULL truth mass. Compare
baseline NDV, common-MCV/residual, and trusted unique-key refinements. No common MCV or
statistically disjoint domain may become semantic proof.

LEFT JOIN fixtures independently estimate matched pairs and left rows with at least one
match, and assert output cardinality never falls below the preserved-left estimate unless
that input is itself proven empty. Add controlled DISTINCT and GROUP BY fixtures for one
column, NULL grouping, and multiple columns; compare the §35.21–§35.23 NDV/damping rules
with reference group counts while retaining heuristic provenance.

---

## Cardinality-Estimator Quality Benchmarks

Apply the shared Chapter-42 benchmark evidence policy to a predeclared, multi-case
deterministic campaign per workload family; retain generator parameters/seeds, schema,
query/predicate/join shape, snapshot, exact fixture/reference result, analyzed SchemaVer
and StatsVersion, estimator configuration, and estimate confidence/provenance. Use the
existing Selectivity Estimation Tests, Join Estimation Tests, V35, and Chapter-34 owners
for formula/SQL correctness. An analytically known cardinality or independent reference
execution under the same snapshot supplies actual rows/pairs, including duplicate and
NULL join semantics; the production estimate or EXPLAIN actual field is not its own
oracle. Hold statistics generation stable within a comparison. Label stale, missing,
rejected, or fallback statistics as distinct strata rather than mixing them invisibly
with current complete descriptors.

For every cardinality-comparable case retain raw estimated rows `E`, independently
observed actual rows `A`, Chapter-40 q-error, family, shape, statistic identity,
provenance/confidence, and any limitation label. Use §40.8's exact rule: positive `E,A`
give `max(E/A,A/E)`; both zero give `1`; exactly one zero gives an explicit infinity.
Never clip or drop infinity. Not-started, partial, demand-censored, failed, or cancelled
operator observations are not comparable q-error samples under §40.6; use a complete
reference result or explicitly classify unavailability. A statistical zero is still
only an estimate; approved §35.2/Chapter-20 semantic-empty proofs retain their separate
provenance and are reported separately where their output boundary differs.

Report case count, median/p50, predeclared upper-tail quantiles (for example p90/p95),
maximum/infinite cases, and a histogram/CDF or retained raw values, separately or
stratifiably by family and estimate provenance. Identify worst cases with query shape,
`E`, `A`, q-error, StatsVersion, and limitation reason. A mean may supplement but never
replace the distribution. Use enough predeclared distinct cases per family to expose a
tail, not a single representative; fixed-seed randomized cases may supplement exact
hand-built fixtures. No universal q-error acceptance threshold is imposed here.

- Baseline/uniform, skewed hot-value/long-tail and nonuniform-range fixtures retain exact
  frequencies so uniform-assumption error is visible. MCV fixtures distinguish a proven
  MCV member, a non-MCV residual value, multiple hot values, and common-MCV versus
  residual join mass; membership comes from the independent statistics fixture, not low
  observed error. Reuse the canonical MCV correctness owner.
- NULL cases cover no-NULL, NULL-heavy and applicable all-NULL populations with `IS NULL`,
  `IS NOT NULL`, nullable equality/range, IN/three-valued logic, and nullable join keys.
  Record exact NULL fractions and reference cardinalities; catalog NOT NULL proof remains
  distinct from observed/statistical zero. Reuse existing selectivity/join fixtures.
- Correlation-limitation cases use deterministic positively correlated, anti-correlated,
  or functionally related columns whose individually plausible single-column statistics
  cannot describe joint predicate frequency. Label them `MODEL-LIMITATION` under §35.17
  but retain their comparable q-errors in the family distribution, even when large.
  Do not manufacture multi-column extended statistics or excuse difficult cases as
  N/A. This inter-column limitation is distinct from Chapter-36 index/heap correlation.

Controlled unique-to-many, many-to-many, hot-key, partial-overlap, and duplicate-heavy
join cases enrich the campaign without turning it into join-order performance testing;
optional GROUP/DISTINCT cases use their existing reference-group oracles. Record whether
MCV, histogram, NDV, NULL fraction, uniqueness, independence/multicolumn damping, exact
constraint, or missing/stale fallback contributed to each estimate, using Chapter-35
owner tags rather than inferring provenance from the number. Distributional quality,
base-access cost choice, and measured execution speed remain separate conclusions.

---

## Access Path Tests

Construct catalog/stats scenarios where optimizer should choose:

```text
SeqScan for low-selectivity predicate
IndexScan for highly selective predicate
IndexScan for ORDER BY avoidance
SeqScan when index correlation is poor and result is large
IndexScan when correlation is high
```

Vary one architecture-owned input at a time:

```text
relation rows and physical pages
predicate selectivity and row width
required/projected payload width
dead-version and physical index-entry pressure
index/heap correlation
effective-cache and calibrated page/CPU cost configuration
required ordering
```

Inspect every enumerated SeqScan/usable IndexScan alternative, its bounds/residuals,
estimated rows/width, physical candidate pressure, provided ordering, and component costs.
Logical selectivity remains common to all physical algorithms; only physical work differs.

Use two fixtures with the same selectivity but different table size, correlation, required
width, ordering, or configured costs and require different legitimate path choices. This
proves no universal selectivity threshold controls access selection. Planning must use the
configured cache model rather than momentary BufferPool residency.

For an index `(a,b,c)`, cover equality prefixes, one range after an equality prefix,
inclusive/exclusive endpoints, duplicates/RID bounds, `IS NULL` on a nullable key, ordinary
`= NULL`, an unconstrained suffix, a later-key predicate without its leading prefix, and
residual predicates. Compare cursor results with the normalized SQL predicate and assert
exact leftmost-prefix bounds, transient sentinel use, and residual classification under
§§36.11–36.14; `IS NULL` may form its exact key bound while `= NULL` may not.

Do not assert arbitrary exact cost numbers unless testing the cost formula itself.

Prefer plan-shape expectations under controlled parameters.

For Chapter-42 measured base-access calibration, additionally apply the shared benchmark
policy to deterministic same-logical-shape query families with validated immutable
TableId/IndexId/SchemaVer, complete statistics generation, and one retained validated
CostConfig (including deployment-calibrated §36.4 weights). For each condition, execute
*both* a direct validated `PhysicalSeqScan` and the relevant legal `PhysicalIndexScan`
with the same snapshot, predicate, output, order requirement, data, and comparable full
physical-operation start/completion window. Reuse V22/V27, MVCC/index and Access Path
correctness oracles: both results must equal an independent SQL reference result before
their performance samples count. The unselected alternative is measured too; optimizer
choice or abstract cost alone is not a physical measurement. Cost remains an abstract
weighted resource model, not predicted milliseconds.

Predeclare controlled series for every §42.5 dimension, making each the principal
changed variable while recording unavoidable couplings:

| Dimension | Replayable fixture and independent activation evidence |
|---|---|
| Selectivity | Same predicate form with varied literal/range parameters and exact fixture/reference qualifying counts over snapshot-visible rows; include narrow, intermediate, and broad cases without a universal percentage cutoff. |
| Heap pages / rows per page | Comparable live-row counts with deliberately different actual heap-page extents/physical-version density; observe pages, row/version counts, and occupancy rather than deriving pages from nominal width. |
| Index/heap correlation | Same leading-key distribution placed in high-locality and poorly localized heap-page order; record canonical `leading_key_heap_correlation`, candidate RIDs, distinct heap pages reached, and actual locality/I/O, not insertion order alone. |
| Cache budget | Vary configured `effective_cache_pages` in its canonical page unit so estimated I/O terms demonstrably consume the assumption; hold/record execution BufferPool capacity, preconditioning and hit/miss regime separately. The planner never reads momentary residency. |
| Required output width | On a wide relation hold predicate/index/layout fixed and vary RequiredSlotSet/ColumnIds from narrow to wide; record canonical estimated row width, actual decoded/materialized columns, CPU/memory, and similar or changed page I/O. |
| VARCHAR width | Fixed-seed short/long payload classes with recorded actual stored/logical widths, comparison/copy work, rows/page and I/O; qualify any coupled page-density change rather than attributing it solely to decode width. |
| Dead-version fraction | Stable or comparable visible rows plus controlled MVCC obsolete/dead versions; report owner-observed dead/invisible physical versions divided by relevant physical tuple versions, along with both counts, pages, index garbage/candidates, and MVCC rejects. This fraction is a Verification fixture measure, not a new persisted statistic. |

For each alternative retain elapsed time, measured CPU scope, logical/physical rows,
SeqScan pages and tuple versions considered, visible and output rows, required decode/
materialization, and same-window BufferPool hits/misses and completed physical reads
where applicable. For IndexScan also retain physical entries/candidate RIDs, distinct
heap pages touched, stale/invisible rejects, residual checks, and heap MVCC checks;
logical qualifying rows are not candidate pressure. Source these from Chapter-7/27/40
owners, not a wrapper or abstract cost. Record actual selectivity from fixture truth,
actual physical density from heap observations, VARCHAR payload width rather than
`sizeof(StringRef)`, and RequiredSlotSet width rather than full schema width. A changed
VARCHAR width may change rows/page; narrow projection need not change scan-page I/O.

At each condition record validated CostConfig identity, selected TABLE/INDEX StatsVersion
and analyzed schema, freshness/fallback provenance, `effective_cache_pages`, actual
BufferPool state, actual selectivity/pages/rows-per-page/correlation/output and VARCHAR
width/dead fraction, SeqScan and IndexScan measured work, both alternatives' estimated
rows/physical candidate pressure/component costs, required/provided ordering, and the
optimizer-selected legal path. Statistics must correspond to the changed physical data;
an intentional stale-statistics case is separately labeled. Direct measurements and
cost/plan evidence are distinct. Optional ordering-benefit and combined-dimension
diagnostics may supplement, not replace, the seven one-principal-variable series.

Include a positive same-shape switch series with measured condition sets on opposite
sides of a real cost break-even: one selects SeqScan, another IndexScan, both legal and
both measured under each key condition. Verify the chosen path follows the lower valid
abstract cost under the active objective, not a wrapper's expected label or a hard-coded
selectivity threshold; observe the changed physical/resource conditions that caused the
component-cost change. No specific literal, percentage, or machine-independent switch
point is prescribed. A slower selected path in a noisy run is not semantic failure;
stable disagreement is retained as §36.4 calibration evidence, without silently changing
weights, declaring global optimality, or beginning G10 operator-cost ranking.

---

## Join-Order Tests

Use known cardinalities where one join order is dramatically better.

Example:

```text
A = 1M rows
B = 1M rows
C = 100 rows

A ⋈ B produces huge intermediate
B ⋈ C highly selective
```

Verify DP chooses the selective early join when statistics indicate it.

Also test a case where a bushy plan wins.

Generate small join graphs and compare enumerated legal trees with an independent exhaustive
reference enumerator. Use distinct BindingIds for self-joins. Cover connected and
disconnected graphs, a query that semantically requires a Cartesian product, and a
connected graph where unnecessary Cartesian partitions must not displace connected
alternatives. Cartesian cost may be high but legality is never changed by a blanket ban.

Build LEFT JOIN regions with inner joins on each side. Assert the outer-join boundary stays
atomic/constrained, legal inner reordering occurs within each side, and a lower cost cannot
authorize crossing the boundary. Differentially execute representative legal reorderings;
inject illegal reorderings only to assert final rejection before execution.

For INNER equijoins, force cases where build-left and build-right are respectively cheaper
and assert both supported orientations are enumerated. For LEFT hash join assert only the
architecture-supported preserved/probe orientation appears. Also compare NestedLoop,
HashJoin, capability-enabled MergeJoin, and INLJ without changing the shared logical join
cardinality. INLJ fixtures include a tiny selective outer with a useful inner index and a
large/unselective outer where repeated lookups lose.

---

## Physical Property and Enforcement Tests

For each OrderingProperty, vary slot identity, direction, NULL order, collation, and prefix
length. Assert exact-prefix satisfaction, rejection of near-matches, and advertised order
only from runtime-capable providers. RequiredSlotSet fixtures retain predicates, join keys,
grouping/aggregate inputs, ordering slots, output values, and hidden DML state while pruning
unused payload.

### Interesting orders and enforcement

Verify optimizer can retain a slightly more expensive ordered path and use it to avoid:

```text
Sort
```

or enable:

```text
MergeJoin / streaming aggregate
```

when total plan cost is lower.

Compare an unordered cheapest child plus Sort with a more expensive naturally ordered
IndexScan/join/aggregate path. Assert the ordered alternative survives dominance when it
reduces total downstream cost. When no child supplies the exact property, require explicit
Sort with startup, CPU, memory, and spill cost; when the property is already satisfied,
reject redundant Sort enforcement.

For ORDER BY with LIMIT/OFFSET, compare full Sort + Limit against Top-N using small exact
`K`, mathematical `K` above `INT64_MAX`, no LIMIT, incompatible order, and different
memory targets. When its implementation supports exact `K`, Top-N must produce a member of
the same Chapter-20 permitted ordered-result family as full Sort + Limit; different actual
occurrences and relative orders inside a complete-key boundary tie class are legal. Top-N
must be ineligible otherwise and may be selected only when legal before the active objective
cost is compared. Exact-K infeasibility is never a public count-overflow error; V22-J owns
the mathematical oracle and fallback matrix.

For GROUP BY and DISTINCT, compare hash implementations with capability-enabled ordered/
streaming implementations under unordered input, already compatible ordering, required
downstream order, and constrained memory. Include any necessary Sort enforcement in the
total cost; an implementation's local operator cost is not compared as if properties were
free.

For measured interesting-order evidence, predeclare one same-logical-query fixture with
identical required final OrderingProperty and two validated complete alternatives: a
locally cheaper unordered provider plus explicit Sort, and a locally more expensive
ordered provider that avoids that Sort (or enables another supported ordered operator).
Structured cost observations must show `ordered local > unordered local` yet `ordered
complete < unordered + enforcement complete`; a locally cheaper ordered provider is not
this positive control. Execute both complete plans on the same data, CostConfig,
statistics, memory budget, and environment. Record elapsed time, CPU, peak query memory,
Sort comparisons/work, and applicable spill for each, with direct evidence that the
ordered plan avoids enforcement. Reuse the property and V30 ordered-result oracle:
incidental row order is not a provided property, and both alternatives must satisfy the
same direction/NULL/collation/tie semantics. A runtime reversal is calibration evidence,
not a correctness failure or a required universal speedup. Optional MergeJoin/ordered
aggregate benefits may supplement Sort avoidance but are not mandatory.

---

## Memory/Spill Plan Tests

With identical statistics but different query memory budgets:

```text
large budget
small budget
```

verify plan selection may change between:

```text
HashAggregate vs SortAggregate
HashJoin vs alternative
in-memory Sort vs spill-aware costs
```

where supported.

Also compare `ALL_ROWS` with a safely propagated `FIRST_K_ROWS(K)` objective for the same
logical query. Verify low-startup and low-total-cost alternatives can differ, blocking
operators do not pretend all work scales with K, and objective identity participates in
memo lookup where propagated. `FIRST_K_ROWS(0)` remains costing metadata, not semantic
emptiness.

With absent, rejected, stale, and low-confidence statistics, assert centralized finite
fallback assumptions, explicit confidence/provenance, deterministic planning, retained
runtime alternatives, and semantic correctness. A slower fallback plan is acceptable; a
fabricated proof is not.

For measured memory-aware plan choice, keep the same logical query, data/schema,
StatsVersion, cardinality/selectivity estimates, CostConfig, properties, and capability
set. Vary the retained §38.19 `query_execution_memory_budget_bytes` (the per-query soft
execution-memory input) and record each blocker's assigned target, predicted need/peak,
estimated spill bytes/passes and cost components, all legal alternatives, and selected
plan. Hold §38.21 `optimizer_planning_arena_budget_bytes` fixed and nonbinding: its guard
or search fallback is not execution-memory plan-choice evidence. Choose an implemented
pair with different memory/spill behavior—HashAggregate versus a supported ordered
alternative, HashJoin versus a legal join, Sort/TopN, or another owned pair. Include a
positive large/constrained-budget series where estimated spill changes *and* a legal
selected plan changes; stable-plan cases are supplementary, not a substitute. Directly
execute the chosen plan and key legal alternative where practical under each budget,
checking independent result semantics and QueryMemoryManager peak, owner spill bytes,
runs/partitions, CPU, I/O, and elapsed time. Require owner-observed positive spill in at
least one constrained spill-capable control; record no/reduced spill at the larger budget
where practical. Predicted/actual disagreement is calibration evidence, not a fabricated
success or a failed SQL result. Controlled OutOfMemory is an owner failure, not a valid
positive plan-choice sample. Neither budget is a universal cutoff.

---

## Memo and Pruning Tests

Construct memo/search requests that differ only in logical subproblem, required slots,
ordering class, or propagated required-rows objective, and construct alternatives that
differ in feasibility or semantic proof. Exercise insertion, dominance, replacement, and
pruning directly.
Assert:

- the only semantically valid or capability-enabled alternative is never discarded;
- useful ordering and low-startup alternatives survive when required by the active key;
- required slots/properties are never erased by merging distinct states;
- estimated rows and semantic-empty provenance remain orthogonal fields;
- hash-map order, allocator address, worker timing, and pointer values cannot affect
  retention or final selection.

The mandatory estimated-zero regression inserts an executable alternative with
`estimated_rows == 0` and `is_provably_empty == false`, then applies cost ties, dominance,
property pruning, and join-DP transitions. It must remain available whenever required. Run
the selected plan against data containing a visible row/match. A second malformed candidate
that promotes the estimate into proof must be rejected, not allowed to dominate the
executable plan.

---

## Cost Model Tests

For scans, joins, Sort, Top-N, aggregates, DISTINCT, materialization, and spill paths,
evaluate cost components over zero/small/large and boundary-straddling inputs. Assert every
ordinary cost and component is finite and nonnegative, `total = startup + run` for the
full-result objective, and invalid arithmetic cannot leak NaN or an uncontrolled infinity
into comparison. Test exact formulas only with their architecture-owned configured weights;
otherwise assert monotonic relationships and component attribution.

Use otherwise identical plans with large unused columns present/absent from RequiredSlotSet.
Required payload width, decode/materialization CPU, hash state, sort records, and spill
bytes must reflect the pruned plan rather than full table width.

Assign several deterministic query-memory budgets across simultaneously live blockers.
Assert assigned targets do not exceed the phase budget, stable redistribution order is
used, estimated peak is the maximum simultaneous phase rather than a blind sum, and hash,
sort, aggregate, DISTINCT, and materialization spill components respond to the targets.
Required Sort/Top-N/ordered aggregate/DISTINCT enforcement contributes its full startup,
memory, and spill cost before alternatives are compared.

---

## Optimizer Determinism and Resource-Limit Tests

Create exact and near-tied alternatives, then perturb insertion order, allocator layout,
hash seeds/iteration order, and repeat optimization. The canonical structural key must
choose the same plan under §38.4's tie rule. Verify the structural comparison key is
collision-free for the compared alternatives; force or simulate a compact
`PlanFingerprint` collision and prove the hash alone never decides identity, dominance, or
tie order. Fingerprints remain deterministic diagnostics rather than semantic identity or
a cryptographic guarantee.

At, below, and above the exhaustive join threshold, compare explored subsets/partitions
with expected exhaustive or bounded behavior. Independently lower the planning-arena/work
budget through deterministic hooks. Assert transition to the deterministic connected
heuristic, bounded local-improvement passes, bounded memo/arena growth, legal complete plan
return when fallback fits, and controlled `OptimizerResourceLimit` when even fallback
cannot fit. Wall-clock duration may be measured diagnostically but is not the correctness
trigger when a deterministic work/memory limit is available.

---

## Final Optimizer Validation Tests

After selection and before handing a plan to execution, invoke optimizer final validation
and then the execution-layer Physical-Plan Validator tests above. Construct one malformed
winner for each §38.24 class:

```text
changed logical output/join semantics or illegal predicate placement
missing required output/hidden RID slot
unsatisfied or falsely advertised ordering
crossed LEFT JOIN boundary or unsupported physical capability
Top-N whose mathematical first-K exceeds its selected implementation's exact domain
negative/nonfinite memory or spill annotation
statistics-derived or otherwise unapproved semantic-empty/no-op proof
estimated-zero base/join subtree with every executable path removed
```

Give each malformed plan the lowest numerical cost. Assert final validation fails,
execution is never invoked, no DML/storage/catalog/WAL effect occurs, and a structured
internal optimizer/validation error is surfaced. Cost cannot excuse an invalid plan.

For the proof boundary, test both forms explicitly:

```text
estimated_rows == 0 + statistics-derived is_provably_empty -> reject
estimated_rows == 0 + is_provably_empty == false + executable path -> accept
```

Positive final plans must preserve output slots, hidden RIDs, join semantics, runtime
capabilities, memory/spill declarations, and every required/provided ordering. Reuse the
§41.5 validator and pre-DML-effect procedures rather than duplicating their malformed-plan
matrix.

---

## Optimizer Differential Correctness Tests

For small random schemas/data:

1. generate supported logical queries,
2. execute optimizer-chosen physical plan,
3. execute a simple trusted reference physical plan, such as:
   ```text
   SeqScan + NestedLoopJoin + straightforward operators
   ```
4. compare each result against the owning semantic result family as described below.

This tests optimizer transformation correctness independently of cost quality.

Include NULL-rich and skewed inputs, stale/missing/rejected statistics, numerical-zero
estimates without proof, different legal join orders/algorithms, interesting-order choices,
and memory-driven plan changes. Normalize only ordering that SQL leaves unspecified.
Use Chapter 22’s physical-shape result-family procedure and V29-H–K for FLOAT64
SUM/AVG and their downstream scalar/relational consequences; a single reference
plan’s selected tree is not the complete oracle for another admitted tree. Compare
exact results where the semantic owner guarantees equality, and independently
check each admitted outcome and controlled error elsewhere. Optimization cannot
introduce outcomes outside the owner contract or change its demand rules.

For statistics/estimate/cost perturbations in V34-078, V35-063/V35-068, and
V36-069, reuse V22’s tagged two-root aggregate composition fixture. Prove that
selected plans actually exercise distinct admitted trees and record the selected
plan, leaf/edge evidence, and independent downstream result for each. A change in
cost metadata with the same tree does not activate this case. Retain V22’s
illegal-root, lost/duplicated-leaf, and exact-COUNT negative controls; legal tree
variation cannot excuse an unrelated value, multiplicity, visibility, or error
ownership defect.

---

## Optimizer Fuzzing

Fuzz:

```text
logical expression trees
predicate combinations
statistics values within legal ranges
join graphs
ordering requirements
required slot sets
semantic-proof metadata
memory budgets
```

Requirements:

```text
no crashes
no NaN/negative costs escaping
no invalid physical plan
bounded planning work/resources above threshold
no semantic proof fabricated from estimates/statistics
```

Generate malformed statistics/proof/property inputs only through paths intended to reject
them, and require bounded structured failure. Record reproducible seeds. Fuzzing supplements
the explicit publication, estimated-zero, memo, and final-validation regressions; it does
not replace them.

---

## Plan Regression Suite

Maintain named optimizer scenarios containing:

```text
schema and stable object identities
statistics descriptor/version and confidence
query/logical input
cost and memory configuration
important semantic/property expectations
canonical plan fingerprint where diagnostically stable
```

Include selective point lookup, large range scan, star/bushy join, skewed hot key, required
Cartesian product, LEFT JOIN boundary, ORDER BY index match, small LIMIT, low-memory spill,
missing/stale statistics, and estimated-zero-without-proof cases. Fix configuration and
assert exact shape only when deliberate; otherwise assert legality, properties, capability,
and semantic result so tests do not overfit incidental cost changes.

## Optimizer Diagnostics Tests

Inspect structured EXPLAIN/trace fields rather than unstable prose. Verify visibility of:

```text
selected StatsVersion and staleness/confidence/provenance
table/column/index pressure inputs and fallback assumptions
predicate TRUE/FALSE/UNKNOWN estimates
estimated rows and row width
is_provably_empty and approved proof provenance as a separate field
enumerated/chosen access bounds, residuals, join order, algorithms, and orientations
major cost components, memory targets, spill expectation, and required/provided properties
dominance pruning, interesting-order retention, and bounded-fallback reason
final plan structural key/fingerprint and planning resource counters
```

For an estimated zero without proof, diagnostics must show both facts distinctly. Compare
estimated rows with actual execution counters through the §41.5 EXPLAIN ANALYZE tests and
verify q-error/reporting does not mutate estimates, proofs, persistent statistics, or future
planning semantics.

---

## Cost Model Benchmarks

For each operator collect actual resource behavior across scales:

```text
SeqScan
IndexScan
HashJoin
NestedLoop
IndexNestedLoop
HashAggregate
Sort
TopN
spill paths
```

Compare:

```text
predicted relative ranking
actual runtime ranking
```

Apply the shared Chapter-42 evidence policy. For each listed operator, predeclare a
deterministic multi-scale physical fixture with input/output cardinalities, key/group and
payload widths, selectivity/skew, required/provided properties, statistics/CostConfig,
vector size, BufferPool setting, query-memory target, capability, and expected result.
Reuse G9's directly measured SeqScan/IndexScan series rather than recreating it; associate
their predicted page/version/candidate/locality/decode terms with owner-observed CPU,
BufferPool/page I/O, rows, and output. Reuse G5 and V27–V30/V24 correctness and positive
operator/spill activation. Each sample needs actual nonzero target-path work and correct
output; plan presence or a benchmark label alone is insufficient. Record exact structured
startup/total costs and major components from the optimizer owner, not rounded EXPLAIN
prose or elapsed time. Keep weights stable during a campaign; a §36.4 proposal follows
the campaign rather than silently tuning every sample to agree.

Compare alternatives only when they satisfy the *same* logical requirement, or compare
one path across controlled scale points; do not globally rank unrelated queries. To
measure an unselected legal alternative, use a validated direct physical fixture or
retained alternative without public optimizer hints or changed production selection.
Keep the timed operation boundary comparable within each family. Record elapsed time,
CPU, page/BufferPool I/O, rows, comparisons/hash work, peak QueryMemoryManager memory,
spill bytes/runs/passes and other applicable Chapter-40/operator counters separately.
Predicted abstract cost is not milliseconds, and measured resources are a vector, not an
invented scalar actual cost. For each pair retain predicted ordering and component basis,
measured runtime order or tie/inconclusive result under G1 variability, measured resource
tradeoffs, correctness, and any disagreement. Never force a winner from overlapping noisy
results or impose a universal ranking-agreement percentage. Summaries by family may count
agreements/disagreements/inconclusive cases for diagnosis, not PASS thresholds.

- HashJoin: vary legal INNER-equijoin build/probe rows, key/payload width, duplicates,
  skew, output, memory and spill regime; record build/probe/output work, hash CPU, memory,
  spill, and time. NestedLoopJoin: vary outer/inner rows, predicate work/selectivity and
  output, observing repeated comparison/evaluation work. Directly compare both for at
  least one same logical join with meaningful scale variation, without a fixed crossover.
- IndexNestedLoopJoin: use a legal indexed inner, varying outer size/selectivity; count
  index probes, candidate RIDs, heap MVCC checks, matches, BufferPool/I/O, CPU and time.
  Compare with HashJoin/NestedLoopJoin for the same join where each is legal; all three
  need not coexist in every fixture.
- HashAggregate: control input rows, groups, key/state width, skew, memory target and
  spill; compare predicted update/finalize/memory/spill terms with actual work. Where
  supported, compare a complete legal ordered/sort-aggregate alternative, including
  enforcement and retained-output cost; unsupported optional operators are N/A.
- Sort: use disordered inputs varying rows, key/payload width, comparison shape and
  memory, recording comparisons where available, runs, merge passes, spill, memory, CPU
  and time. TopN: use legal exact-`K` ORDER BY LIMIT/OFFSET cases varying input, `K`,
  key/row width; compare complete TopN and Sort+Limit alternatives under the same
  Chapter-20/V30 ordered-result family, including permitted boundary ties. Ineligible
  TopN is not a calibration alternative.
- Spill paths: for supported Sort, HashJoin, and/or HashAggregate fixtures, include
  in-memory and positive owner-observed spill cases for the same operator requirement.
  Compare predicted spill/merge/repartition/temporary-I/O components to owner bytes,
  passes, memory, CPU and elapsed work. A correct spill is valid execution, not failure;
  an annotation or temp-file final size alone does not prove spill activity.

This comparison records relative predicted cost versus measured runtime *and* each
applicable resource dimension. Persistent disagreement informs §36.4 calibration; it
does not invalidate a correct plan or require that one abstract cost unit equal a time
unit. No unsupported algorithm, global operator tournament, or perfect concordance is
required.

---

## Optimizer Performance Benchmarks

Measure planning time for join counts:

```text
2
4
6
8
10
12
16
20
30
```

Track:

```text
subsets explored
partitions considered
physical plans costed
memo entries
peak planning memory
```

Verify exhaustive search transitions to bounded heuristic behavior.

Treat the grid above as Verification-owned supplementary fixtures, not an Architecture
join-count mandate. For the core transition curve choose and record a valid nonzero
`exhaustive_join_limit` with enough planning-arena budget to avoid guard fallback. Count
distinct BindingId occurrences in one maximal legal reorderable region: §37.11 selects
exhaustive bushy DP for `N <= limit` (including equality) and §37.13 bounded heuristic
for `N > limit`. Zero is legal for ordinary heuristic-only configuration but cannot
demonstrate both modes. Use a deterministic connected INNER-join graph family with
recorded BindingIds, predicates, table statistics/StatsVersions, CostConfig, required
properties, and resource configuration. Include below, at, immediately above the active
limit and larger heuristic counts; add/substitute boundary-adjacent points if the grid
does not bracket the chosen limit. Optional graph classes remain separate.

Measure planning wall time with a monotonic timer from canonical optimizer invocation
entry through validated physical-plan completion, excluding execution. From the *same*
invocation's Chapter-37/38 owner trace record configured threshold, region size, initial
and selected search mode/transition reason, distinct logical join subsets explored by
BindingId set, legal subset partitions considered, physical alternatives that actually
reached costing (not merely enumerated), and memo alternatives/entries retained versus
pruned by dominance as two separately scoped counts. State whether a counter denotes
groups, alternatives, or entries; do not derive partitions from subsets or pruned from
created-minus-final unless the owner event semantics make that exact. Peak planning
memory is the simultaneous live byte peak of the dedicated §38.21 planning arena, not
RSS, execution query memory, or final live bytes. Reuse its lifecycle/cleanup tests.

For heuristic points record greedy connected-tree extension/candidate work and local-
improvement passes, legal moves considered, and accepted improvements from the §37.13
owner. One larger case must show nonzero heuristic work; `large_join_max_local_passes=0`
may truthfully give zero optional local-improvement work, whereas a configured active
pass fixture records actual pass/move evidence without fabricating accepted changes.
Direct search-mode events must prove both sides of the threshold and a completed legal
plan; fast wall time, relation count alone, or planning-arena fallback does not prove
the intended transition. Resource-guard fallback and OptimizerResourceLimit belong to
separate correctness/resource diagnostics, not this core threshold curve. Keep required
wall time, subset, partition, costed-alternative, retained/pruned memo, peak-arena, and
heuristic-work evidence together by optimizer invocation identity; missing fields follow
G1's NOT VERIFIED disposition. The Star Schema Benchmark remains optional SHOULD-level
supplementary stress, not official TPC compliance or a substitute for this curve.

---

## Star Schema Benchmark

Even though the engine is general purpose, use a synthetic star schema to stress:

```text
many joins
selective dimensions
large fact table
aggregation
```

This is an excellent optimizer-learning workload.

TPC-H-inspired queries may be added without claiming benchmark compliance.

---

## No Benchmark Gaming

Optimizer decisions must not recognize benchmark-specific raw SQL bytes or hashes,
whitespace/comments, query numbers, suite/case labels, ordinary user table/index/column
names, magic fixture object IDs, or known logical-plan/join-graph fingerprints to select
a special rewrite, access path, join order, algorithm, property, memory strategy, or
search path. This list is not exhaustive. Decisions instead follow general statistics,
semantic rewrites, properties, costing, and search (§42.6). Names remain valid for
binding, catalog lookup, EXPLAIN, errors, and diagnostics; stable TableId, ColumnId,
IndexId, BindingId, and LogicalSlotId remain valid for lookup, semantic identity,
structural keys, and deterministic ties. Architecture-owned handling of system objects
is outside the ordinary-user-name oracle. Generic sargability, rewrite, connectivity,
star-topology, and interesting-order rules are valid when their preconditions and
resulting decisions are general, correct, and explainable through those owners. A
branch recognizing a particular benchmark query/shape or magic user ID to bypass
ordinary costing/search is not such a rule.

Use paired metamorphic *invariance* and *sensitivity* controls, plus generated holdouts
and a supplementary read-only review of production optimizer decision code. For each
variant retain raw fixture/query identity, bound and normalized logical identity,
declared semantic-ID bijection if any, schema/StatsVersion and statistics values,
CostConfig, execution and planning memory settings, required properties, capabilities,
search configuration, candidate set, structured cost components, optimizer trace/mode
and counters, selected structured plan, and correctness result. Keep harness case labels
outside the optimizer invocation. Record a trace-delta ledger of changed identity,
estimates, properties, costs, search inputs, and selection; do not infer cause from the
selected plan alone. Reuse Optimizer Diagnostics Tests for owner trace evidence and
the Plan Regression Suite for named scenarios, without overfitting incidental costs.

For *exact-input* controls, produce raw SQL with different bytes but owner-certified
identical bound/logical optimizer input: whitespace, legal comments, keyword case,
formatting or harmless parentheses where valid under Chapter 18/20; rename aliases
only when the binder establishes the same semantics. Owner-certified logical
normalization variants may supplement these, but do not assume reordered executable
conjuncts, reversed comparisons, or textual INNER-join order are equivalent: Chapter
20's demanded evaluation, source-error provenance, predicate occurrences, and join
graph must be checked first. Compare candidate set, cost components, search mode and
decision, and selected plan exactly, excluding only source spans, raw text, and display
diagnostics. A formatting-only SourceSpan shift is not a physical-plan input. The
different-byte/same-input pair detects raw-query-text or raw-hash selection without
requiring a production hash. Parser/binder possession of raw SQL is not itself gaming.

For *isomorphic-input* controls, clone ordinary user schemas/queries with unrelated
table, index, and column names, neutral and benchmark-like names, alias changes, and
rekeyed TableIds/ColumnIds/IndexIds/BindingIds under a declared bijection. Include a
name-swap control that preserves mapped statistical/semantic roles. Hold mapped types,
constraints, statistics, physical pressure, index properties, CostConfig, memory,
capabilities, and search settings equivalent. Use a strict unique cost winner and
compare legal access/join/property candidates, exact structured costs, and selected
operator/access/join/property/enforcement/memory-spill structure modulo the mapping;
do not require raw IDs or compact fingerprints to match. This exposes magic IDs or
benchmark-like spelling without prohibiting semantic ID lookup. An exact tie can
legitimately select another mapped tied representative under §38.4's structural key;
compare its legal cost-equivalence class and tie rule, not identical mapped selection.
Bounded heuristic search can also depend on canonical structural ordering: use
name-only identical-input variants for the strongest invariance oracle and demand
mapped equivalence only where a strict winner and the search comparison are well
founded. Do not infer mode from time or require identical incidental trace iteration.

Include fixed-seed isomorphic many-join/star variants with unrelated names/IDs and
equivalent graph, fact/dimension statistics, and selective dimensions. A general
topology/statistics heuristic may make the same mapped choice across these; a special
known-shape override is forbidden. Generate multiple additional non-benchmark names,
aliases, formatting variants, isomorphic schemas, and controlled parameter changes
from a recorded recipe, not one hand-picked rename or secret cases. For any
benchmark-motivated improvement, include non-benchmark holdouts satisfying the same
general preconditions; the mechanism need not yield equal speedup or improve every
holdout. A fixed-seed cost-margin family stays within a predeclared strict-winner
region, requiring the same mapped choice without a universal numeric margin.

For *sensitivity*, hold benchmark identity fixed and cross a genuine owner-defined
decision boundary by changing legitimate inputs. Reuse G9's measured selectivity,
density, correlation, cache, required width, VARCHAR, or dead-version controls for
base access; reuse G10's join costing, interesting-order, memory/spill, and
exhaustive/heuristic configuration controls as applicable. Include at least one
boundary-crossing holdout with a cost/trace-supported plan or mode change and one
invariance control where only forbidden identity/presentation changes. Record whether
statistics/estimates, semantic rewrite preconditions, required/provided properties,
capabilities, CostConfig/cost components, execution-memory/spill estimates, search
configuration/state, or deterministic tie rules explain each decision. A benchmark-
shaped plan frozen across a strict general-input crossover is adverse evidence, just
as an unexplained plan change under identity-only variation is a benchmark-gaming or
generality failure. Do not demand a particular speedup; G1/G9/G10 own performance
comparability and uncertainty. A legitimate input change need not always change plan.

Compare canonical structured plans and candidate/cost/search evidence, not the compact
`PlanFingerprint` alone. Chapter 38 permits it for diagnostics and regression comparison,
not semantic identity, costing, dominance, ties, or special-plan selection; reuse the
Optimizer Determinism and Resource-Limit Tests' forced-collision case. Check mapped
logical subproblems, physical operators and child orientation, access class, properties,
enforcement, memory/spill form, and cost components. If only forbidden presentation or
benchmark identity changes, an unexplained candidate, cost, search, or plan delta fails;
investigate canonical determinism rather than dismissing it as hash-order noise.
All variants must pass binder/rewrite/plan-validation correctness and, when execution
semantics need checking, the appropriate differential/reference result oracle. Pure
decision-invariance cases need not all execute after equivalence is established.
Optimizer Fuzzing may add reproducible identifier permutations and controlled statistics
perturbations but cannot replace the explicit positive controls.

When implementation exists, supplement behavior with a narrow read-only review of
production rewrite/cost/search decision branches for benchmark names/query numbers,
raw SQL comparisons/hashes, magic ordinary-user IDs, and exact known graph/plan-shape
fingerprints. Classify each match: tests, comments, benchmark harnesses, fixture
definitions, catalog binding, and diagnostic name display are not decision branches.
A static string scan alone cannot exclude encoded hashes or indirect shape/ID cases and
cannot establish generality. Do not add a public anti-gaming API, persistent benchmark
IDs/fingerprints/allowlists, hidden-test requirement, or external benchmark-compliance
claim; this procedure defines Verification/CI evidence, not a stored format.
