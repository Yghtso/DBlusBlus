# 25. Vectorized Expression Execution

## 25.1 Expression execution state

Bound/physical expressions are translated into reusable per-execution expression state.

The execution contract is conceptually:

```text
Evaluate(
    expression_state,
    input DataChunk,
    active selection
)
    -> Vector result
```

For each invocation, the caller supplies or semantically determines the
demanded subset of the input's active logical occurrences under §§20.6–20.7
and 20.17. Each parent derives each child's demanded subset from that subset
using Chapter 17's scalar control flow and §20.17.5's executable child order.
Demand composes recursively: a child cannot regain an occurrence removed by an
ancestor condition, and vector-layout convenience cannot omit a demanded child
occurrence. An empty demanded subset creates no per-row semantic evaluation;
§17.10.2 remains authoritative for binding-time constant folding.

The active selection represents this logical demand, not a second semantic
rule or a required mask API. A SelectionVector, bitmap, index list, partitioned
evaluation, or another exact representation MAY realize the same demanded
occurrences. Every occurrence must already belong to the input's active
logical domain under §§23.1, 23.6, and 23.8; a demand representation cannot make
inactive capacity, stale slots, or out-of-domain dictionary positions reachable.
Physical speculation outside semantic demand cannot contribute observable
values or ordinary errors, including §25.1.1 candidates. Within the demanded
domain, Chapter-17/20 value, NULL, child-order, and error rules remain exact.

Operator/type/representation dispatch occurs at expression/vector-batch granularity. The executor does not construct one generic `Value` object per active cell in hot loops. Physical expression state is query-execution state and does not mutate the immutable bound/logical expression meaning.

An expression-subquery node dispatches through §20.14.7's occurrence-keyed
side-plan state rather than recursively running its child once per vector row.
If the active selection first demands an uninitialized occurrence, execution
runs/finalizes that side plan once, then supplies a CONSTANT result vector or
vectorized IN probe for precisely the demanded rows. Empty/skipped selections
do not initialize it.

### 25.1.1 Ordinary non-DML runtime expression-error selection

An **ordinary non-DML expression-error candidate** exists for each semantically
demanded scalar evaluation occurrence whose canonical Chapter-17 and Chapter-20
evaluation reaches an ordinary runtime scalar semantic error. Demand is the
semantic property defined by §20.17, not physical evaluation or discovery. An
undemanded CASE branch, short-circuited AND/OR operand, skipped IN-list work,
per-row expression with no demanded row occurrence, or speculative undemanded
evaluation creates no candidate even if a physical kernel computes an error.

Within one demanded scalar occurrence, Chapter 17 and §§20.17 and 20.17.5 remain
authoritative for child order, short-circuiting, CASE/AND/OR/IN demand and order,
and the responsible failing subexpression. This section compares only the
occurrence-level ordinary failures already established by those rules.

Every candidate conceptually retains its canonical responsible source-derived
expression occurrence and diagnostic provenance, responsible `SourceSpan`,
public top-level error category, closed Chapter-17 conceptual cause, and any
other diagnostic metadata already frozen by its owner. A logical demanded row
occurrence establishes candidate membership and multiplicity, but is not a
ranking key, SQL ordering key, or new row identifier. No particular candidate
structure is required.

Among ordinary non-DML candidates, the public error is the minimum under this
lexicographic semantic preorder:

1. smallest responsible `SourceSpan.start_byte_offset`;
2. for equal starts, the shorter represented `SourceSpan`;
3. for identical represented spans, canonical semantic source-expression
   occurrence order preserved through parsing, binding, and logical rewriting;
4. for the same semantic expression occurrence, this closed conceptual-cause
   order:

```text
INVALID_CAST
NUMERIC_OVERFLOW
DIVISION_BY_ZERO
INVALID_DATE
INVALID_TIMESTAMP
```

The fourth component is §25.1.1’s semantic ranking over the existing Chapter-17
cause vocabulary. It does not redefine those causes or their §39 public-category
mappings, and it is independent of numeric enum values, source declaration
order, registry/container iteration, or function-table address. The component
is necessary because one semantic expression occurrence can produce distinct
Architecture-visible causes on unordered rows: for example, the same integer
division may produce `DIVISION_BY_ZERO` or `NUMERIC_OVERFLOW`.

Two candidates are observationally equivalent only when their semantic
diagnostic origin, `SourceSpan`, public category, conceptual cause, and every
other Architecture-frozen diagnostic field agree. Any representative of the
minimum equivalent class is conforming. Exact message prose and generic
offending-value rendering are not generally frozen fields and do not break a
tie.

This is a total preorder over observable candidate classes, not a total order
over rows. Unordered bags remain unordered, and even ordered relational input
does not by itself create §25.1.1 row precedence. Logical row sequence, scan
order, page or RID, physical slot, selection index, and dictionary child index
never rank candidates. Distinct Project expressions are ranked by their
canonical source-derived semantic occurrence, not physical Project visitation;
multiple demanded Filter-row failures use the same provenance/cause rule.

Vector lane, vector capacity, DataChunk boundary, FLAT/CONSTANT/DICTIONARY
representation, selection storage, scan task, worker, thread, SIMD completion
order, pointer or allocation address, hash order, filesystem order, and physical
expression visitation order likewise never select or break a tie. Repeated
DICTIONARY occurrences remain repeated demanded occurrences; when all frozen
candidate fields agree, their candidates occupy the same equivalence class.
Absent a different legitimate resource or cancellation event, legal changes to
these physical choices cannot change the selected ordinary error class.

An implementation MAY vectorize, use SIMD, evaluate semantically demanded work
in parallel or speculatively, accumulate local minima, and merge minima in any
discovery order, provided the selected public error is the same semantic
minimum. A semantic error produced by speculative undemanded work is discarded
because it is not a candidate. Full candidate materialization and scalar
row-at-a-time execution are not required. Execution may stop once it proves
that no semantically smaller candidate class can still be established; first
physical discovery alone is not that proof, and the rule requires no work that
upstream semantics do not demand.

This preorder does not rank DML candidates, including `RETURNING`; §21.16.1
remains authoritative. Aggregate-finalization ranking remains §29.3-owned, and
scalar-subquery cardinality and other specialized subquery precedence remain
§20.14-owned. `OutOfMemory`, `SpillIOError`, `QueryCancelled`, runtime
representability/resource `ExecutionError`, corruption, and internal invalid-
plan/runtime states retain their Chapter-24/§39 or other specialized owners and
do not enter this ordinary-candidate preorder. This section defines no global
precedence between an ordinary semantic error and those independently owned
failures.

### 25.1.2 DML expression-error handoff

Expression evaluation in a DML statement, including candidate-row construction
and RETURNING, preserves the Chapter-18/19/20 diagnostic provenance required by
§21.16.1: the responsible SourceSpan, semantic expression origin, public
category and owning cause, and membership in its scalar/subquery/final-row
expression-evaluation phase. Chapter 21 determines candidate eligibility and
ranking, including attempt and target-revalidation prerequisites; Chapter 25
does not rank NOT NULL, UNIQUE, or other constraint phases.

Execution MUST preserve every eligible expression candidate that the DML owner
needs to select its canonical error. It MUST NOT reduce DML candidates using
§25.1.1's non-DML preorder or physical visitation order in a way that discards
such a candidate. Candidate transport and aggregation have no prescribed
structure. Physical sharing or duplication preserves the upstream semantic
origin under §20.17; node pointers, kernel-table entries, output ordinals, and
vector lanes cannot replace it. RETURNING remains within the DML error owner
and retains §21.15's unordered-bag semantics.

For a write-DML attempt that reaches persistent publication, all semantically
demanded candidate-producing expression work, including INSERT source/target
conversion, UPDATE assignments, and RETURNING, has completed before the first
persistent user-DML write under §§15.1.3 and 21.16.1. Vectorization, sharing,
or lazy side-plan initialization cannot defer a demanded ordinary DML
expression candidate into the mutation/write-publication phase. This rule does
not demand speculative or Chapter-20-undemanded evaluation.

## 25.2 Input normalization

An executable input reference consumes the resolved mapping from its semantic
LogicalSlotId under §20.2 to the containing operator's physical input schema
under §22.3. For schema entry `S[j]`, §23.1 supplies `DataChunk.columns[j]` of
the matching runtime type. Column ordinal `j` is a runtime locator within that
schema, not semantic identity. Projection reordering and derived-table remapping
use the corresponding schema mapping; equal-valued or repeated outputs and
self-join inputs remain distinct through their upstream slot identities.
Execution does not rediscover identity from a column name, BindingId, raw
ordinal alone, vector position, or pointer, and vectors need not duplicate the
schema's LogicalSlotIds.

A fixed-width/vector kernel:

1. resolves its physical type/operator implementation once,
2. obtains UnifiedVectorFormat for each input,
3. loops over the active logical rows,
4. writes/references one result Vector.

The effective input index for each active logical row comes from the normalized selection view. NULL validity is read through the normalized validity view. Kernels therefore produce representation-independent results for FLAT, CONSTANT, and DICTIONARY inputs.

On successful ordinary scalar evaluation, there is exactly one logical result
occurrence for each demanded input occurrence, with the corresponding mapping,
order, and multiplicity, unless an owning specialized expression/operator
contract says otherwise. CONSTANT's single payload does not collapse its
logical occurrences; repeated DICTIONARY selections produce repeated results.
The result may use any legal vector representation that preserves this mapping.
The scalar executor does not remove rows, deduplicate, or reorder occurrences:
§§20.6 and 27.7 own Filter row removal, including when a predicate kernel directly
produces a selection; §§20.7, 22.3, and 27.8 own Project output placement and
schema identity. Computing an intermediate or result Vector creates no new
LogicalSlotId; temporary expression vectors need no independent semantic slot.

## 25.3 Arithmetic kernels

For fixed-width arithmetic, provide a specialized no-NULL path:

```text
if every required input is all-valid:
    FastKernel
else:
    NullableKernel
```

Result validity follows Chapter 17's expression semantics. The kernel uses the resolved physical type directly; it does not switch on SQL type once per row when the batch-level operation is already known.

Arithmetic kernels execute the scalar semantics owned by §§17.4.3 and
17.6.1–17.6.2 over demanded logical occurrences; §§39.3.1–39.3.2 own their
runtime error enforcement. Kernel specialization MUST preserve those semantics.
Resolved casts and comparisons likewise consume §§17.7–17.8 and §19.6 without
new type or overload resolution. The closed scalar-function scope remains
§17.9.3-owned.

## 25.4 Comparison kernels

Comparison kernels produce SQL BOOLEAN results with nullable validity where ordinary comparison may yield NULL. For filter contexts, a comparison kernel MAY write matching logical row positions directly to a SelectionVector instead of materializing an intermediate BOOLEAN vector.

VARCHAR comparison uses prefix/length fast rejection where valid and full unsigned byte comparison when required. FLOAT64 comparison remains compatible with Chapter 17/Chapter 8 semantics.

## 25.5 Vectorized AND short-circuit

For `A AND B`, evaluate `A` first.

This is the physical realization of the mandatory per-row §17.7.3 evaluation
order, including its error-suppression boundary.

Per logical row:

```text
A = FALSE:
    result is definitely FALSE
    do not evaluate B for this row

A = TRUE or NULL:
    row enters B evaluation subset
```

After B is evaluated for that subset, combine according to SQL three-valued logic.

## 25.6 Vectorized OR short-circuit

For `A OR B`, evaluate `A` first.

This is the corresponding mandatory §17.7.3 OR rule, not an optional kernel
optimization.

Per logical row:

```text
A = TRUE:
    result is definitely TRUE
    do not evaluate B for this row

A = FALSE or NULL:
    row enters B evaluation subset
```

Then combine with SQL three-valued logic. These selection-based rules preserve the architecture's volatility/error-evaluation boundary: an expression branch that SQL semantics permit to skip is not eagerly executed merely because evaluation is vectorized.

## 25.7 Result ownership

A computed fixed-width result uses reusable output vector storage owned by the expression/operator state or output chunk. A computed VARCHAR result owns/copies its bytes into storage whose lifetime covers the returned result vector, normally the output chunk StringHeap for streaming expressions.

A simple column reference or other exact reference result MAY borrow vector or
string backing under §§23.10 and 23.12. The borrowed logical view remains
value-stable for its complete consumer interval, including all reachable
payload, validity, selection, representation metadata, StringRef metadata, and
referenced bytes. Under §23.13, an owner cannot reset, reuse, or incompatibly
mutate that backing until the borrow ends or an exact §23.12 mechanism preserves
every live view. This also governs input/output aliasing: an in-place write
cannot change an input still needed by a child or consumer.

Section 26.6 owns the synchronous pipeline-consumption interval, which permits
zero-copy borrowing. A consumer retaining the result beyond that interval
obtains stable ownership or materializes it under §§23.10–23.12 and the
retaining operator's contract. No particular copy, transfer, or copy-on-write
mechanism is required. Computed VARCHAR bytes that do not borrow stable backing
have an exact result owner for the full required lifetime under §§23.11 and
27.8.

### 25.7.1 Valid expression state and result publication

Execution consumes the resolved physical-plan contract in §§22.2–22.3 and 22.8,
with the final-plan validation handoff in §38.24. A wrong child count,
unresolved or missing kernel for a validated executable expression, input or
output TypeId mismatch, or invalid input-slot mapping is an internal invalid-
plan/runtime state. Out-of-domain selections, invalid validity/representation
state, expired or unstable borrows, and uninitialized demanded output are
internal vector/lifetime violations under §§23.6, 23.10, and 23.14. These states
are not user TypeError, CastError, ArithmeticError, or ordinary error candidates.
The empty scalar-function registry in §17.9.3 supplies no executable named
scalar-function state; generic IR capacity does not authorize such a kernel.

Construction invariants, final-plan validation, and appropriate local runtime
guards MAY establish these preconditions without revalidating every expression
tree for every vector call. Malformed state MUST be prevented or rejected
before unsafe pointer/range calculation, out-of-bounds access, dangling
StringRef dereference, stale-value publication, arbitrary mutation, or
persistent database effects. Internal failures follow §39.1's invariant-failure
consequences; assertion failure is not permission to continue with invalid data.

Successful evaluation initializes every demanded result occurrence's validity,
exact non-NULL payload, and required variable-length ownership. Undemanded or
inactive output capacity is semantically inaccessible. If evaluation fails,
the invocation MUST NOT publish its result vector/view as a successful
expression result or expose incomplete demanded positions as consumable active
output. This local publication rule does not retract earlier query chunks
already returned under §31.10. A valid expression's allocation,
representability, or cancellation failure remains its canonical resource or
cancellation error under §24.10 and §39.3, not malformed internal state.

### 25.7.2 Runtime representation and resource handoff

Section 23.9 owns exact runtime VARCHAR representability. Compact StringRef's
UINT32_MAX domain and persistent heap-tuple capacity do not impose SQL VARCHAR
or expression-result maxima. An exact supported alternate runtime form remains
applicable when the compact form cannot represent the value; no truncation,
wrap, or clipping is permitted. A retaining consumer must separately satisfy
§24.2's exact retained-row applicability, because an exact scalar form does not
make a narrower retained descriptor capable of storing it.

Expression scratch, temporary vectors, demand-selection storage, and computed
output bytes use the runtime owners in §22.6 and the continuously accounted
owner regions in §§24.4–24.5. Aggregate capacity that grows with execution
cannot bypass accounting through individually small kernel allocations or
untracked scratch; capacity already covered by a parent region needs no
duplicate object charge. For kernels that produce variable-length values,
including scalar-to-VARCHAR casts, output lengths, offsets plus lengths,
allocation extents, and capacity growth obey §24.1's exact-before-use rule.

Sections 23.9, 24.10, and 39.3 distinguish unsupported exact runtime
size/address/value representation (controlled representability/resource
ExecutionError) from catchable allocation denial for a supported exact form
(OutOfMemory). QueryCancelled retains its cancellation owner. These operational
limits affect feasibility without redefining Chapter-17 values, and add no
precedence relative to ordinary expression errors.

## 25.8 Expression invariants

1. Expression execution is vectorized.
2. Generic Value construction is not a hot per-cell representation.
3. Vector kind normalization happens outside the per-row representation branch.
4. All-valid inputs have specialized validity-free paths where useful.
5. Comparison preserves SQL NULL semantics.
6. Filter contexts may produce selections directly.
7. AND/OR short-circuiting evaluates only the rows whose second operand is semantically required.
8. Vectorized short-circuiting does not eagerly execute skippable volatile/error-producing branches.
9. VARCHAR/FLOAT64 comparison semantics agree with the type/index contracts.
10. Computed varlen results own bytes for their required lifetime.
11. Aggregate argument vectors supply every successfully evaluated demanded typed occurrence to §29.3 exactly once; vector shape may select a legal FLOAT64 reduction tree but cannot change admission, multiplicity, or another aggregate's semantics.
12. Only semantically demanded ordinary non-DML runtime expression failures enter §25.1.1's candidate preorder; Chapter-17/20 child order first determines each occurrence-level failure.
13. Ordinary non-DML error selection uses source-derived provenance and the closed conceptual-cause order, never row, lane, chunk, representation, worker, address, or physical visitation order.
14. DML, specialized aggregate/subquery, resource, cancellation, corruption, and internal-invalid-state errors retain their separate canonical owners.
15. Child demand composes within the parent's active logical domain under Chapter-17/20 semantics; physical mask representation cannot add or omit observable evaluations.
16. DML expression provenance and eligible candidates reach the §21.16.1 owner without non-DML or physical-order preselection.
17. Resolved input slots use the physical-schema/DataChunk mapping; successful scalar results preserve demanded occurrence mapping and multiplicity without creating slot identity.
18. Malformed expression/vector state is rejected or prevented, and failed evaluation never publishes an incomplete result as successful.
19. Borrowed results preserve the full value-stable view through consumption and owner reset/reuse under §§23.10–23.13 and 26.6.
20. Exact runtime/retained representation, extent arithmetic, and accounted memory remain §§23.9 and 24.1–24.5 responsibilities, with errors classified by §24.10 and §39.3.
