# Live Documentation Reference Cleanup

## Scope

Audited and cleaned:

- `ARCHITECTURE.md`
- `PROJECT_STATE.md`
- `DEVELOPMENT.md`
- `VERIFICATION.md`
- `AGENTS.md`

## Changes

- Removed all `_Source: archived architecture §..._` provenance lines from `DEVELOPMENT.md` and `VERIFICATION.md`.
- Removed live-document wording that described those guides as extracted from archived/source material.
- Reworded stale `Next Architecture ...` development headings as development-stage guidance tied to the current `ARCHITECTURE.md`.
- Renamed `Modules Added by This Architecture` to a normal recommended module-layout heading.
- Replaced the stale early `PROJECT_STATE.md` claim that RID reserved bytes were an open architecture question with the correct implementation-mismatch classification.
- Removed architecture-rewrite/cutover provenance wording from the current-state and final-architecture status text.
- `AGENTS.md` contained no legacy/rewrite-source reference and was left semantically unchanged.

## Residual legacy/rewrite-reference audit

PASS — no legacy/rewrite-source provenance patterns remain in the five cleaned live documents.

## SHA-256 comparison

| File | Original | Cleaned | Changed |
|---|---|---|---|
| `ARCHITECTURE.md` | `0365df9e9aa094f4e91e15cbddcedf8fc44d3dd1b75e781ec51eda5fcb4b8a82` | `3a1b5ad668168e0f3c6f704eb1fc026b73fd24eea6a6785e08c83a03a1ce2531` | yes |
| `PROJECT_STATE.md` | `161af3d761bb25491198262b8d7d871d5f819ce9b0f8d9ee6c2c30bbae3bc250` | `d1d9869071c820f03e06d3d6b46bc779e9827046868d95cac1d7d69f5d218a3c` | yes |
| `DEVELOPMENT.md` | `02ea8000597385ec97621d5a7bf93b33b195a87f413c4017081630bf77f3ab67` | `888d440604cf88b0e5cfc5e8f5093d44cbbb18832ae3b25577858a4d9486e3ef` | yes |
| `VERIFICATION.md` | `db5f5f407596b2dc9176841ff669071f8ab12b87f81bcce097f7dde8cf9b32af` | `1ba99912437f8a5e3d21d51fbe8e1a9e0e8d6ec0577bf86fec09ce5d23ae3e10` | yes |
| `AGENTS.md` | `df5f654a8505e35afc35675ca77b831a3b082d9ad61c9b3c14ddb0a3650e477e` | `df5f654a8505e35afc35675ca77b831a3b082d9ad61c9b3c14ddb0a3650e477e` | no |
