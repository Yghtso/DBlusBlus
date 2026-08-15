# Rewrite Pass 16 — Whole-Document Reconciliation and Cutover

## Scope

Pass 16 reconciles the entire rewritten architecture against the pinned legacy source.

Inputs:

```text
ARCHITECTURE(4).md
SHA-256 2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86

ARCHITECTURE_NEW_PASS15.md
SHA-256 35decf86d6f0cc4244789bdd36e1844244bfcdf0b3c2a1f6d9d9112e05d03281
```

Final replacement:

```text
ARCHITECTURE.md
SHA-256 0365df9e9aa094f4e91e15cbddcedf8fc44d3dd1b75e781ec51eda5fcb4b8a82
```

## Work completed

1. Verified every legacy numbered section `0..725` has a final disposition.
2. Reconciled duplicated normative ownership without changing semantics.
3. Audited persistent numeric codes, byte offsets, widths, checksums, formulas, and sentinels.
4. Validated all internal section cross-references and numbered heading hierarchy.
5. Removed rewrite-process, legacy-section, AI/agent, milestone-status, and stale project-state wording from the technical architecture.
6. Preserved implementation sequencing/module-layout guidance in `DEVELOPMENT.md`.
7. Preserved detailed test/crash/fuzz/benchmark procedures in `VERIFICATION.md`.
8. Updated `PROJECT_STATE.md` so the RID reserved-byte item is correctly described as an implementation mismatch rather than an open architecture question.
9. Updated `AGENTS.md` to use the final architecture's normative-language model and supporting-document roles.
10. Replaced the pre-cutover `ARCHITECTURE.md` only after the final audit gates passed.

## Semantic coverage result

```text
legacy sections        726
accounted              726
pending                0
missing IDs            0
duplicate IDs          0
```

Final destinations:

```text
ARCHITECTURE.md                         639
ARCHITECTURE.md + VERIFICATION.md       50
DEVELOPMENT.md                           32
architecture + PROJECT_STATE/devlog      4
AGENTS.md                                1
```

## Persistent-format audit

```text
automated contract checks: 144
passed:                    144
failed:                    0
```

The audit covers the page/file/type/WAL registries and every byte-exact format indexed by Appendix A, plus the FSM/status mapping formulas and representative boundaries.

## Duplicate normative ownership resolved

Canonical ownership was clarified for:

```text
B+ node reserved/flag validation
WAL-before-data
terminal transaction publication vs integrated COMMIT/ABORT
catalog cache publication/lifetime
detailed test/benchmark procedures
```

Cross-layer summaries now point to the detailed owner rather than restating an independent normative rule.

## Cross-reference / terminology result

```text
unresolved §x.y refs       0
duplicate numbered IDs     0
heading-parent violations  0
stale Pass/rewrite wording 0
Codex/agent architecture wording 0
```

## Open questions

```text
unresolved v1 architecture questions: 0
```

R-001 remains open only as an implementation/state mismatch:

```text
architecture:
    RID reserved bytes 14..15 must be zero
    v1 decoder rejects nonzero

current Phase-1 implementation checkpoint:
    decoder remains permissive
```

No production code was changed.

## Documentation cutover

The final roles are:

```text
ARCHITECTURE.md   authoritative technical contract
PROJECT_STATE.md  current implementation state
DEVELOPMENT.md    implementation roadmap/module guidance
VERIFICATION.md   detailed verification/benchmark procedures
AGENTS.md         AI/agent workflow and reading rules
devlog/           append-only engineering history
```

## Cutover result

**PASS 16 COMPLETE — CUTOVER ACCEPTED.**

The archived legacy source remains unchanged.

This documentation cutover does not authorize or begin implementation Phase 2.
