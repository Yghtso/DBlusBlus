1. **M-1 original:** “concurrent transactions and, later, parallel query execution.”

2. **M-1 corrected:** “concurrent transactions and a parallel-ready query-execution architecture that permits an initial single-worker baseline.”

3. This matches §26.9 and Chapter 32: the execution model remains parallel-ready and capable of multiple workers while an initial one-worker executor remains permitted.

4. **M-1:** RESOLVED.

5. **M-2 original terminology:** “Non-goals for the initial major version,” “The initial major version,” and “initial architecture scope.”

6. **M-2 corrected terminology:** “Non-goals for v1,” “The v1 architecture,” and “the v1 architecture baseline.”

7. Chapter 1 now consistently uses the canonical `v1` scope terminology established by the front matter.

8. **M-2:** RESOLVED.

9. **M-3 original:** “These areas are outside the initial architecture scope and MAY be explored after the single-node engine is mature.”

10. **M-3 corrected:** “These areas remain deferred from the v1 architecture baseline unless promoted by an explicit architecture revision.”

11. The non-goal list and deferred scope are unchanged.

12. The maturity condition and roadmap sequencing are removed.

13. **M-3:** RESOLVED.

14. **Remaining temporal terms:**

   - `current` implementation capabilities: valid document-role wording.
   - `current` architecture contract: valid canonical-contract wording.
   - `later promoted`: valid explicit architecture-revision semantics.
   - `initial single-worker baseline`: durable execution-model freedom.
   - `Initial implementations` and improvements `later`: durable evolution principle.
   - `deferred`: durable v1 scope.
   - `future proposal` and `current and proposed design`: valid revision governance.
   - No remaining `mature`, `maturity`, `previous`, or `next` wording in the reviewed slice.

15. **Version terminology:** Consistent. The front matter remains `Authoritative v1 architecture contract` / `Architecture version: v1`; §1.3 now uses `v1` and `v1 architecture baseline`.

16. **Parallel scope:** Consistent with §26.9 and Chapter 32. The wording neither defers parallelism outside v1 nor requires multi-worker execution from the first implementation.

17. **Appendix C:** Consistent. The same capabilities remain deferred/non-v1 unless explicitly promoted.

18. **Architecture semantic questions:** None.

19. **Architecture requirements changed:** No.

20. **Normative strength changed:** No. The removed `MAY` governed roadmap exploration; the replacement restates the existing Deferred/revision contract without changing scope.

21. **Architecture version/status changed:** No.

22. **Files changed:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

23. **Changed hunks:**

   - §1.1 capability-list line: M-1.
   - §1.3 heading/opening sentence: M-2.
   - §1.3 closing scope sentence: M-3.
   - No unrelated changes.

24. **`git diff --check`:** Passed with no output.

25. **AGENTS.md:** Unchanged.

26. **README.md:** Unchanged.

27. **PROJECT_STATE.md:** Unchanged.

28. **DEVELOPMENT.md:** Unchanged.

29. **VERIFICATION.md:** Unchanged.

30. **Chapter 2+:** Unchanged.

31. **Source, tests, benchmarks, and build configuration:** Unchanged.