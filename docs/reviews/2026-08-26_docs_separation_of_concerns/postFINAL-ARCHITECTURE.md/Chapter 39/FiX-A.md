## 1–6. Repository and diff

  1. Repair result: N39-1 and N39-2 are repaired. This is not an independent Architecture closure.
  2. Initial HEAD: 20ee5e00255e7fa7d75b6b3ee2c5b4372fc4b1d5
     Commit: 20ee5e0 chapter 39 ARCHITECTURE analysis

     This differs from the reported 7d2c878... baseline. The intervening commit adds only the historical,
     nonnormative Chapter-39 ANALYSIS.md artifact. It was preserved and not modified.

  3. Initial state: tracked worktree clean; index clean.
  4. Final HEAD: unchanged at 20ee5e00255e7fa7d75b6b3ee2c5b4372fc4b1d5.

     Final status:

      M docs/ARCHITECTURE.md

     Index remains clean.

  5. Task-modified file: only docs/ARCHITECTURE.md.
  6. Diff size: 35 insertions, 12 deletions.

  Exact changed passages:

  - §39.1.1, lines 29012–29018: command outcomes and independent result dimensions.
  - §39.1.5, lines 29231–29241: read-only C4 outcomes, C6 mappings, and the read-only publication boundary.
  - §39.1.6, lines 29276–29277: A4 success and transport-failure outcomes.
  - §39.1.7, lines 29296–29353: connection-loss windows and error precedence.

  ## 7–17. N39-1 — command outcomes

  ### Original defect

  SUCCESS previously described only an ordinary statement leaving an explicit transaction ACTIVE, or an
  autocommit statement proceeding toward COMMIT. It did not represent successful explicit COMMIT or ROLLBACK.
  C6 and A4 transport failures also lacked a complete relationship among server-side completion, terminal
  state, response delivery, and client uncertainty.

  ### Final command-outcome inventory

  The existing five outcomes remain unchanged:

  - SUCCESS
  - FAILED_TRANSACTION_REMAINS_ACTIVE
  - FAILED_TRANSACTION_MUST_ABORT
  - COMMIT_OUTCOME_UNCERTAIN
  - DATABASE_NONCONTINUABLE

  No new public outcome or transaction state was introduced.

  SUCCESS now means the requested operation completed semantically on the server:

  - ordinary explicit-transaction statement → ACTIVE;
  - autocommit statement → implicit COMMIT completed through C4–C5;
  - explicit COMMIT → COMMITTED;
  - explicit ROLLBACK → ABORTED.

  Command completion, transaction state, database continuation, and response delivery are explicitly
  independent.

  ### Acknowledgement loss

  - C6 COMMIT acknowledgement lost: actual state remains COMMITTED; command outcome is COMMIT_OUTCOME_UNCERTAIN
    with ConnectionFailure; no success response is claimed.

  - A4 ROLLBACK acknowledgement lost: actual state remains ABORTED; server-side ROLLBACK completed with
    SUCCESS, but ConnectionFailure records that no success response was delivered.

  - Automatic ABORT A4 loss: retains the originating FAILED_TRANSACTION_MUST_ABORT result and chains the
    transport failure.

  - COMMIT_OUTCOME_UNCERTAIN is not reused as a generic ROLLBACK transport result.

  ### Complete request-outcome matrix

   Case                Server completion and state       Command outcome                      Delivery/
                                                                                              database/cleanup
  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━
   A. Explicit         Complete; ACTIVE                  SUCCESS                              Success
   SELECT succeeds                                                                            response;
                                                                                              continuing
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   B. Explicit         Complete; ACTIVE                  SUCCESS                              Success
   INSERT succeeds                                                                            response;
                                                                                              effects remain
                                                                                              transaction-
                                                                                              local
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   C. Autocommit       C4–C5 complete; COMMITTED         SUCCESS                              Success response
   SELECT succeeds                                                                            after implicit
                                                                                              COMMIT
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   D. Autocommit       C4–C5 and prepared R complete;    SUCCESS                              No premature
   DML succeeds        COMMITTED                                                              RETURNING
                                                                                              exposure
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   E. Explicit         COMMITTED                         SUCCESS                              C5 complete; C6
   COMMIT ack                                                                                 delivered
   delivered
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   F. Explicit         ABORTED                           SUCCESS                              A3 complete; A4
   ROLLBACK ack                                                                               delivered
   delivered
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   G. Ordinary         ACTIVE                            FAILED_TRANSACTION_REMAINS_ACTIVE    Statement
   failure before W                                                                           cleanup;
                                                                                              database
                                                                                              continues
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   H. Ordinary         MUST_ABORT → ABORTED              FAILED_TRANSACTION_MUST_ABORT        Automatic A0–A4
   dynamic failure
   after W
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   I. Independently    MUST_ABORT → ABORTED              FAILED_TRANSACTION_MUST_ABORT        Independent of W
   transaction-
   fatal error
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   J. Database-        Noncontinuable                    DATABASE_NONCONTINUABLE              Controlled stop/
   fatal statement                                                                            recovery
   error
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   K. COMMIT fails     Mandatory ABORT                   FAILED_TRANSACTION_MUST_ABORT        No COMMIT
   before                                                                                     success
   authorizing
   boundary
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   L. Append/          COMMITTING; recovery decides      COMMIT_OUTCOME_UNCERTAIN             Connection-
   durability                                                                                 fatal; generally
   uncertain                                                                                  noncontinuable
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   M. COMMIT           COMMITTED                         SUCCESS                              C5–C6 complete
   reaches
   COMMITTED; ack
   delivered
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   N. COMMIT           COMMITTED                         COMMIT_OUTCOME_UNCERTAIN             ConnectionFailur
   reaches                                                                                    e;
   COMMITTED; ack                                                                             reconciliation
   lost                                                                                       required
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   O. ROLLBACK         ABORTED                           SUCCESS                              A3–A4 complete
   reaches ABORTED;
   ack delivered
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   P. ROLLBACK         ABORTED                           Server-side SUCCESS                  ConnectionFailur
   reaches ABORTED;                                                                           e; no delivered
   ack lost                                                                                   success
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   Q. Automatic        ABORTED                           Original                             Original error
   ABORT completes                                       FAILED_TRANSACTION_MUST_ABORT        returned
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   R. Automatic        Noncommit retained                Original MA plus chained cleanup/    May become
   ABORT fails                                           fatal cause                          noncontinuable
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   S. Database         Canonical evidence determines     COMMIT_OUTCOME_UNCERTAIN where no    Noncontinuable
   becomes             actual outcome                    acknowledgement exists               additionally
   noncontinuable                                                                             reported
   during COMMIT
  ──────────────────  ────────────────────────────────  ───────────────────────────────────  ──────────────────
   T. Database         Never COMMITTED                   Explicit rollback: NC; automatic     Recovery/
   becomes                                               abort: original MA plus NC           controlled stop
   noncontinuable
   during ABORT

  N39-1 disposition: CLOSED.

  ## 18–32. N39-2 — read-only COMMIT and connection loss

  ### Original defect

  Chapter 15 permits a read-only transaction to elide C2–C3, but Chapter 39’s COMMIT matrix previously
  described C4 failure only through the persistent transaction’s durable C3 point. The connection-loss matrix
  also omitted read-only C4 and terminal COMMITTED/ABORTED cleanup windows.

  ### Canonical read-only path

  The repaired rule preserves:

  C0 → C1 → C4 → C5 → C6

  For read-only COMMIT:

  - C2–C3 are elided.
  - No TXN_COMMIT record is invented.
  - No commit_lsn or durable_lsn event is invented.
  - C4’s atomic §9.14 terminal-publication linearization is the publication-authorizing and irreversible COMMIT
    boundary.

  - C5 remains mandatory before acknowledgement.

  ### Read-only C4 outcomes

   C4 condition                                           Required result
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Before C4, or exact coherent nonpublication            Retain ownership and retry, or terminate COMMIT
   established                                            through mandatory ABORT
  ─────────────────────────────────────────────────────  ──────────────────────────────────────────────────────
   C4 result/coherence cannot be established              Database noncontinuable; COMMIT_OUTCOME_UNCERTAIN;
                                                          preserve locks/registry ownership
  ─────────────────────────────────────────────────────  ──────────────────────────────────────────────────────
   C4 atomically publishes COMMITTED                      Irreversible COMMITTED; continue C5; never ABORT
  ─────────────────────────────────────────────────────  ──────────────────────────────────────────────────────
   Failure observed after C4 publication                  A C5/C6 failure, not a prepublication failure

  This distinguishes retry, legal prepublication abort, noncontinuability, and irreversible terminal success
  without prescribing one catch-all result.

  ### Persistent C3/C4 preservation

  Persistent COMMIT remains unchanged:

  - valid publication-authorizing append makes COMMIT uncancellable;
  - durable C3 makes the semantic outcome irrevocably COMMITTED;
  - C4 failure after C3 makes the database noncontinuable but cannot cause ABORT;
  - C5/C6 failure cannot reverse durable COMMIT.

  ### Complete COMMIT matrix

   Path/stage                           WAL/durability                      Terminal state and command result
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Persistent C0/C1 failure             No commit record                    Mandatory ABORT; never success
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C2 known no-append        Exact restoration                   Mandatory ABORT
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C2 append uncertain       Unknown persisted prefix            NC + COMMIT_OUTCOME_UNCERTAIN
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent append valid, status      Commit may survive                  NC + uncertainty; never ABORT
   publication fails
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C3 retryable failure      Bytes retained                      Remain COMMITTING; retry
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C3 cannot be              Recovery decides                    NC + uncertainty
   established safely
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent durable C3; C4 fails      Durable COMMIT                      COMMITTED + NC
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C5 safe fallback          Durable COMMIT                      COMMITTED; success may follow
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C5 incoherent             Durable COMMIT                      COMMITTED + NC
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C6 delivered              Durable COMMIT                      SUCCESS
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Persistent C6 lost                   Durable COMMIT                      COMMIT_OUTCOME_UNCERTAIN +
                                                                            ConnectionFailure
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C0/C1 failure              No WAL applicable                   Mandatory ABORT
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C2/C3                      Elided                              No fabricated durability
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only pre-C4 exact               No terminal publication             Retry or mandatory ABORT
   nonpublication
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C4 indeterminate/          No fictitious WAL evidence          NC + COMMIT_OUTCOME_UNCERTAIN
   incoherent
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C4 published               Authoritative runtime               Irreversible COMMITTED
                                        publication
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C5 failure                 Shared C5 rules                     COMMITTED; safe fallback or NC
  ───────────────────────────────────  ──────────────────────────────────  ────────────────────────────────────
   Read-only C6 lost                    Shared C6 rule                      COMMIT_OUTCOME_UNCERTAIN +
                                                                            ConnectionFailure

  ### Complete ABORT matrix

   Condition                               Required result
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Explicit ROLLBACK from ACTIVE           A0–A4; ABORTED; SUCCESS if acknowledged
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   ROLLBACK joining MUST_ABORT             Join mandatory cleanup; retain original failure
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   Automatic ABORT                         Original MA result; not a successful user ROLLBACK
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   Known A1 no-append                      Remain ABORTING; retry; never restore commit eligibility
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A1 append uncertain                     Noncontinuable; recovery loser handling
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A2 publication failure                  Noncontinuable; no false lock release
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A2 succeeds                             Authoritative ABORTED
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A3 succeeds                             Required locks/resources released
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A3 incoherent failure                   ABORTED plus noncontinuable
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A4 delivered after explicit rollback    SUCCESS; ABORTED
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A4 lost after explicit rollback         Server-side SUCCESS; ABORTED; ConnectionFailure
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   A4 lost after automatic abort           Original MA; ABORTED; chained ConnectionFailure
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   Disconnect before A2                    Continue abort; retain ownership
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   Disconnect after A2, before A3          Continue A3; never reactivate
  ──────────────────────────────────────  ──────────────────────────────────────────────────────────
   Disconnect after A3, before A4          Close connection; remain ABORTED

  ### Complete connection-loss matrix

   Window                              Required handling
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   ACTIVE, no COMMIT                   Automatic ABORT
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   MUST_ABORT                          Continue/join ABORT
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   Persistent pre-append COMMITTING    Cancel COMMIT and ABORT
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   Persistent post-append, pre-C3      Continue COMMIT or enter recovery/NC
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   Persistent post-C3, pre-C4          Semantically COMMITTED; finish C4–C5 or NC
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   Read-only before C4                 Cancel COMMIT and ABORT; no fake WAL
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   Read-only during C4                 Complete/retry; exact nonpublication may abort; publication commits;
                                       indeterminate result is NC
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   COMMITTED, pre-C5 completion        Continue C5; retain required ownership
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   COMMITTED, post-C5/pre-C6           Close connection; remain COMMITTED; client uncertain
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   COMMITTED, response delivered       Close; no semantic change
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   ABORTING, pre-A2                    Continue publication and cleanup
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   ABORTED, pre-A3 completion          Continue A3; retain locks until release point
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   ABORTED, post-A3/pre-A4             Close; remain ABORTED; no delivered success
  ──────────────────────────────────  ─────────────────────────────────────────────────────────────────────────
   ABORTED, response delivered         Close; no semantic change

  N39-2 disposition: CLOSED.

  ## 33–36. Preserved contracts

  - Error precedence: durable persistent COMMIT, authoritative read-only C4 COMMIT, mandatory noncommit, and
    authoritative ABORTED publication cannot be rewritten by later cleanup or transport failures. Original
    causes remain primary; cleanup/fatal causes are chained.

  - Locks and caches: ownership remains through terminal publication and then through C5/A3 as required.
    Connection loss cannot authorize early release.

  - Statement-write/no-undo: §§39.1.2–39.1.4 are unchanged. FA-before-W, MA-after-W, DML candidate closure, and
    no statement undo remain intact.

  - RETURNING/autocommit: unchanged. Autocommit result publication still follows successful C4–C5; explicit-
    transaction RETURNING remains distinct from later COMMIT; transport loss cannot reverse terminal state.

  ## 37. Adversarial-case matrix

  Legend: S = SUCCESS, MA = FAILED_TRANSACTION_MUST_ABORT, CU = COMMIT_OUTCOME_UNCERTAIN, NC =
  DATABASE_NONCONTINUABLE.

   Case    Owner and stage                             Required outcome                               Status
  ━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━
   A       §39.1.5 C6 delivered                        COMMITTED, S                                   PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   B       §39.1.6 A4 delivered                        ABORTED, S                                     PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   C       §39.1.1 ordinary explicit statement         ACTIVE, S                                      PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   D       §§15.5/39.1.1 autocommit                    C4–C5 complete, S                              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   E       Persistent C6 lost                          COMMITTED, CU + ConnectionFailure              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   F       Read-only C6 lost                           COMMITTED, CU + ConnectionFailure              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   G       Explicit A4 lost                            ABORTED, server S, ConnectionFailure           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   H       Automatic A4 lost                           ABORTED, original MA + transport cause         PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   I       Persistent append uncertain                 CU + NC/recovery                               PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   J       Durable COMMIT cleanup failure              COMMITTED; safe fallback or NC                 PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   K       Read-only prepublication C4 failure         Retry or mandatory ABORT                       PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   L       Failure after read-only C4 publication      COMMITTED; C5/C6 owner                         PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   M       Read-only disconnect before C4              ABORT; no fake durability                      PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   N       Read-only disconnect during C4              Complete/classify; CU if unresolved            PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   O       Read-only disconnect after C4               COMMITTED; continue C5                         PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   P       COMMITTED, C5 incomplete                    Continue cleanup; retain ownership             PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   Q       COMMITTED, pre-C6                           Remain committed; client uncertain             PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   R       ABORTED, A3 incomplete                      Continue A3; retain locks                      PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   S       ABORTED, pre-A4                             Remain aborted; ConnectionFailure              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   T       Persistent disconnect after append          Uncancellable COMMIT/recovery                  PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   U       Persistent disconnect after C3              COMMITTED; never ABORT                         PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   V       ABORTING disconnect                         Continue abort                                 PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   W       Failed transaction enters MUST_ABORT        COMMIT forbidden; automatic ABORT              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   X       Automatic ABORT fails pre-A2                Noncommit + retry/NC                           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   Y       Automatic ABORT fails post-A2               ABORTED + cleanup/NC                           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   Z       Original error then cleanup error           Original primary; cleanup chained              PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AA      Original error then NC                      Original retained; NC additionally governs     PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AB      Read-only reaches C4                        No commit_lsn/durable_lsn; C4 authoritative    PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AC      Ack loss falsely causes ABORT               Explicitly forbidden                           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AD      Disconnect releases locks early             Explicitly forbidden                           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AE      Transport overwrites terminal state         Explicitly forbidden                           PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AF      Ordinary row-five semantic error after W    Still excluded by candidate closure            PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AG      Dynamic cancellation after W                MA and automatic ABORT                         PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AH      Autocommit RETURNING before C4/C5           Existing prohibition unchanged                 PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AI      Persistent C4 adopts read-only rule         Prevented by separate C3 row                   PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AJ      Read-only C4 depends on C3                  Explicitly rejected                            PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AK      New read-only persisted state               None introduced                                PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AL      ROLLBACK mapped to FA                       Explicit S/ABORTED mapping                     PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AM      A4 mapped to CU                             Explicitly prohibited                          PASS
  ──────  ──────────────────────────────────────────  ─────────────────────────────────────────────  ────────
   AN      Terminal disconnect abandons cleanup        C5/A3 continuation mandatory                   PASS

  ## 38–42. Integrity and regression assessment

  - All Chapter-39 normative tables remain present.
  - The statement-error and crash-recovery tables are byte-for-byte unchanged.
  - Only the COMMIT table, A4 rows, connection-loss table, outcome definitions, and directly necessary
    precedence prose changed.

  - Persistent COMMIT, group commit, terminal WAL headroom, status-page ordering, and recovery classification
    remain unchanged.

  - Chapters 31–38 retain their established owners, including Chapter 38’s distinction between
    OptimizerResourceLimit and OutOfMemory.
      - valid prepublication versus post-publication stages;
      - valid semantic-state versus client-observation distinctions;
      - valid recovery and lock-lifetime rules.

  - The changes are timeless, normative, and confined to the two findings. They introduce no new WAL record,
    transaction state, persisted status, SQL semantic, retry protocol, or transport protocol.

  ## 43–47. Final disposition and validation

  - Remaining Chapter-39 findings from this task: none.
  - New semantic questions: none.
  - N39-1: CLOSED
  - N39-2: CLOSED
  - git diff --check: PASS
  - Audit/repair-created files: none.
  - Index: clean.
  - No staging or commit performed.

  Explicit confirmations:

  ARCHITECTURE CHAPTERS 1–38 NOT MODIFIED

  CHAPTER 39:
      ONLY AUTHORIZED PASSAGES MODIFIED

  CHAPTER 40 ONWARD NOT MODIFIED

  VERIFICATION NOT MODIFIED
  DEVELOPMENT NOT MODIFIED
  PROJECT_STATE NOT MODIFIED
  HISTORICAL ARTIFACTS NOT MODIFIED

  IMPLEMENTATION NOT STARTED
  NO BUILD/TEST/SANITIZER/BENCHMARK
  NO STAGING/COMMIT
  CHAPTER 40 REVIEW NOT STARTED

  CHAPTER 39 ARCHITECTURE FIX A COMPLETE —
  READY FOR INDEPENDENT READ-ONLY
  CLOSURE AUDIT