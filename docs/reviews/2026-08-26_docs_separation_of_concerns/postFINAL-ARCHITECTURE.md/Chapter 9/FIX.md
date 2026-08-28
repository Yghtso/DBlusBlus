## 1. Verdict

**CHAPTER 9 TARGETED FIXES — COMPLETE**

```text
CHAPTER 9 ARCHITECTURE: CLEAN
CHAPTER 9 VERIFICATION: SYNCHRONIZATION STILL PENDING
```

## 2–3. Initial repository state

```text
git status --short:           clean
git diff --cached --name-only: empty
HEAD: dda72d7274702dce1b913479b6a78e0bd31fb835
```

The pre-existing repository state was clean and was preserved.

## 4–12. M1 — §9.7.2

Original wording:

> “Initial membership testing MAY therefore use binary search.”

> “A later high-concurrency implementation MAY replace this with a hybrid or different runtime representation…”

Corrected timeless wording at [§9.7.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:7065):

> “The v1 `active` representation is a sorted vector. Membership testing MAY use binary search or another semantically equivalent lookup over that vector; it MUST preserve the captured membership, owner exclusion, snapshot stability, and sorted order used by §9.7.3's `xmin` derivation.”

> “High-concurrency alternative snapshot active-set representations are deferred from the v1 architecture baseline.”

Results:

- Active-set membership semantics: preserved.
- Snapshot-owner exclusion: preserved.
- `xmin` derivation: preserved.
- READ COMMITTED and REPEATABLE READ lifetimes: preserved.
- V1 representation scope: remains a sorted vector.
- Implementation freedom: limited to semantically equivalent lookup over that vector.
- Alternative high-concurrency representations: remain outside v1.
- DEVELOPMENT sequencing: removed.

**M1: RESOLVED.**

## 13–20. E1 — precise ownership references

### §9.1

Original:

> “The later durability/recovery chapters own…”

Corrected at [§9.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6728):

- collaborating durability/reclamation responsibilities: Chapters 12–14;
- WAL records and commit flushing: §§12.2–12.15;
- checkpoints and crash reconstruction: §§13.4–13.19.

### §9.3

Original:

> “the later durability/recovery chapter”

Corrected at [§9.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6871):

- control-file layout and torn-update protocol: §§13.2.1–13.2.4.

### §9.4

Original:

> “the later durability/recovery chapters”

Corrected at [§9.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6928):

- terminal status/WAL mutation: §12.10.5;
- WAL durability and commit coordination: §§12.13–12.15;
- terminal-state crash reconstruction: §§13.12–13.19;
- failure/continuation outcomes: §39.1.

No durability, recovery, commit, or failure protocol was duplicated.

**E1: RESOLVED.**

## 21–30. Documentation-model assessment

| Assessment | Result |
|---|---|
| Project chronology | None in corrected scopes |
| Current implementation narration | None |
| DEVELOPMENT sequencing | Removed |
| VERIFICATION procedure leakage | None |
| PROJECT_STATE facts | None |
| Devlog/history material | None |
| Analytical rationale | Preserved |
| Terminology | Unchanged and precise |
| Normative strength | Preserved |
| Document role | All task-created text classifies as ARCHITECTURE |

Valid transaction-time wording—such as current transaction, later statement, and commits later—was retained.

## 31. Local re-review answers

| # | Question | Answer |
|---:|---|---|
| 1 | “Initial membership testing” sequencing removed? | YES |
| 2 | Later high-concurrency roadmap removed? | YES |
| 3 | Canonical active-set semantics retained? | YES |
| 4 | V1 representation scope unchanged? | YES |
| 5 | Alternative representations remain outside v1? | YES |
| 6 | Owner exclusion unchanged? | YES |
| 7 | `xmin` derivation unchanged? | YES |
| 8 | RC/RR lifetimes unchanged? | YES |
| 9 | §9.1 references precise? | YES |
| 10 | §9.3 references precise? | YES |
| 11 | §9.4 references precise? | YES |
| 12 | Any protocol duplicated? | NO |
| 13 | Any technical behavior changed? | NO |

## 32. Documentation-model answers

| Question | Answer |
|---|---|
| Corrected §9.7.2 contains implementation sequencing? | NO |
| Contains project chronology? | NO |
| Contains current-state narration? | NO |
| Contains VERIFICATION procedures? | NO |
| Contains PROJECT_STATE facts? | NO |
| Contains devlog/history? | NO |
| Semantic freedom expressed timelessly? | YES |
| Valid runtime temporal language preserved? | YES |
| Chapter remains analytical/descriptive? | YES |
| Independent of implementation status? | YES |
| Timeless canonical v1 contract? | YES |

## 33–34. Technical regression

All unchanged:

- TxnId domain, reservation, nonreuse, maximum, and exhaustion;
- CommandId lifecycle and exhaustion;
- transaction state machine and terminal immutability;
- begin/registration semantics;
- snapshot fields and capture synchronization;
- READ COMMITTED and REPEATABLE READ lifetimes;
- transaction-status encoding and lookup precedence;
- commit C0–C6 and durable point C3;
- abort A0–A4 and no physical user-DML undo;
- read-only transaction behavior;
- recovery/status reconciliation;
- shutdown and NONCONTINUABLE behavior;
- failure taxonomy and numeric exhaustion.

Compatibility remains intact with:

- Chapter 3 durable-COMMIT irreversibility;
- Chapter 4 numeric/status foundations;
- Chapter 5 `xmin/xmax/cmin/cmax`;
- Chapter 7 dirty/durable and latch/lock distinctions;
- Chapter 8 index-candidate visibility recheck.

## 35–39. Protected documents

Confirmed task-unchanged:

- `docs/VERIFICATION.md`
- `docs/PROJECT_STATE.md`
- `docs/DEVELOPMENT.md`
- Appendix C
- Chapter 10

## 40. Frozen semantic questions

**NONE.**

## 41. Follow-up verification gap

**PENDING**, covering:

1. `txn_status.dat` byte/mapping matrix;
2. BEGIN/registration versus snapshot capture;
3. terminal publication versus snapshot capture;
4. snapshot representation and lifetime;
5. status lookup precedence;
6. status PAGE_INIT/extension boundaries;
7. torn/stale status versus WAL reconciliation;
8. read-only participation with no terminal WAL/status.

## 42–44. Out-of-scope status

- Appendix C’s “initial sorted-vector model” wording remains unchanged for a future Appendix review.
- Chapter 10’s broad durability/recovery navigation remains unchanged for its direct review.
- Chapter-10 review: **NOT STARTED**.

## 45–47. Diff scope

Changed file:

- [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6728)

Diff:

```text
1 file changed, 6 insertions(+), 8 deletions(-)
```

Hunk classifications:

| Classification | Hunk |
|---|---|
| A | §9.7.2 timeless active-set semantics |
| B | §9.1 precise ownership references |
| C | §9.3 precise control-file references |
| D | §9.4 precise terminal durability/recovery references |
| E | No separate wrapping-only hunk |

Only `docs/ARCHITECTURE.md` was task-modified.

## 48–53. Final repository checks

```text
git status --short:
 M docs/ARCHITECTURE.md

git diff --cached --name-only:
<empty>

HEAD:
dda72d7274702dce1b913479b6a78e0bd31fb835

git diff --check:
passed, no output
```

No external repository changes were observed during the task. No pre-existing material was modified or staged.

No build, test, benchmark, implementation work, staging, commit, devlog, or review artifact was performed.

**Phase 2 remains NOT STARTED / NOT AUTHORIZED.**