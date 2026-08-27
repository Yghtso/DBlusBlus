Completed the two targeted non-semantic corrections in [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12).

1. Original wording: “Project progress belongs in `PROJECT_STATE.md`.”
2. Corrected wording: “`PROJECT_STATE.md` records current implementation capabilities, limitations, architecture/implementation mismatches, and active implementation boundaries.”
3. “Project progress” was inaccurate because `PROJECT_STATE.md` describes present implementation reality, not chronological development history.
4. Architecture authority remains intact, including “Authoritative v1 architecture contract” and authority over intended behavior.
5. Open protocol owner: §3.3.3, “Open preconditions, ordered protocol, and recovery entry.”
6. Shutdown protocol owner: §3.3.6, “Controlled shutdown protocol.”
7. Lifecycle/failure outcome owner: §3.3.7, “Create, removal, crash, and lifecycle errors.”
8. Original §41.3 reference: “crash/failure at every §3.3.7 open/shutdown point.”
9. Corrected reference: “crash/failure at every open/shutdown boundary in §§3.3.3 and 3.3.6, with §3.3.7 lifecycle outcomes.”
10. The original was imprecise because §3.3.7 defines outcomes, while the ordered protocol boundaries reside in §§3.3.3 and 3.3.6.
11. Lifecycle verification semantics did not change; no case or failure boundary was added or removed.
12. No additional direct instances were found. Other §3.3.7 references correctly concern database creation or lifecycle error categories.
13. Out-of-scope observations: none.
14. Architecture requirements changed: no.
15. MUST/SHOULD/MAY strength changed: no.
16. Architecture version/status changed: no; both remain v1.
17. Files changed: `docs/ARCHITECTURE.md` only.
18. `git diff --check`: passed.
19. `docs/VERIFICATION.md`: unchanged.
20. `docs/DEVELOPMENT.md`: unchanged.
21. `docs/PROJECT_STATE.md`: unchanged.
22. `README.md`: unchanged.
23. `AGENTS.md`: unchanged.
24. Source, tests, benchmarks, and build configuration: unchanged.

Nothing was staged or committed; no build or tests were run.
