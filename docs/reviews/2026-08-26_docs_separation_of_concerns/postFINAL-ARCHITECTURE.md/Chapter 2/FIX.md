PASS B targeted fixes are complete. All three findings are resolved without changing architecture semantics.

1. B-M1 original §2.1 wording:

   > The system follows a downward dependency model:

   > The diagram represents dependency direction, not necessarily one runtime call stack.

2. B-M1 original §2.5 wording:

   > The storage stack follows the dependency direction:

   Its arrows ran from foundational providers toward higher consumers—the opposite orientation from §2.1’s claimed dependency direction.

3. Corrected §2.1 wording:

   > The high-level system coordination and representation flow is:

   > In this overview, each downward arrow denotes a high-level handoff or service-consumption relationship from the preceding stage to the stage below; it is not necessarily one runtime call stack or a complete static module-dependency graph.

4. Corrected §2.5 wording:

   > The storage stack is ordered from foundational providers to higher-level consumers:

   > In this diagram, each downward arrow means that the lower layer consumes services or definitions provided by the layer above.

5. The diagrams now explicitly represent distinct relationship types: §2.1 is a high-level handoff/service-consumption overview; §2.5 is foundational-provider-to-consumer layering.

6. This matches the detailed architecture without falsely presenting §2.1 as a complete static module graph or reversing the storage dependency model.

7. B-M1: **RESOLVED**.

8. B-M2 original wording:

   > Lower layers MUST NOT depend on higher-layer syntax or semantic objects.

9. B-M2 corrected wording:

   > Lower infrastructure MUST NOT depend on unresolved higher-layer syntax, semantic coordination, or policy objects, and receiving metadata does not transfer ownership of its semantics. Resolved immutable descriptors, schemas, layouts, types, and identifiers MAY cross a boundary only where the owning architecture contract explicitly permits them.

10. The corrected rule prohibits unresolved syntax, higher-layer policy, semantic coordination, and authority transfer. It permits only architecture-authorized resolved immutable metadata.

11. This is compatible with §5.19: `TupleCodec` may consume physical `Schema`/`Layout` information.

12. This is compatible with §16.6: immutable catalog and schema descriptors may be consumed without transferring catalog authority.

13. This is compatible with §§22.1–22.3: execution consumes resolved physical plans, immutable descriptors, types, identifiers, and `LogicalSlotIds` without rebinding SQL.

14. B-M2: **RESOLVED**.

15. B-m1 original wording:

   > Normal resident-page access flows through the BufferPool once buffer management exists.

16. B-m1 corrected wording:

   > Normal resident-page access flows through the BufferPool.

17. The BufferPool production-path rule is now timeless.

18. The qualifier “Normal” continues to preserve architecture-defined bootstrap, recovery, or private raw-I/O exceptions.

19. B-m1: **RESOLVED**.

20. Remaining temporal/sequencing wording in Chapter 2 is legitimate:

   - “Later chapters” — document forward reference.
   - “from the start” — durable architectural constraint.
   - “first correct implementation” / “later removal of contention” — durable implementation freedom in §2.4.

21. Diagram and prose are now consistent.

22. The dependency model remains higher semantic layers consuming or coordinating lower services while lower infrastructure remains independent of unresolved syntax and higher-layer policy.

23. No subsystem ownership changed; metadata consumption still does not confer semantic authority.

24. Semantic-diff check:

   - Previously valid detailed-architecture implementation made invalid: **No**.
   - Previously forbidden detailed-architecture implementation made valid: **No**.

25. No frozen architecture semantic question arose.

26. No architecture mechanism changed.

27. No authoritative owner changed.

28. No normative semantic strength changed beyond clarifying B-M2 to match existing detailed contracts. B-m1 only removed implementation-stage wording.

29. Architecture version and status are unchanged.

30. Pass-A corrections remain intact: parallel-ready wording remains, canonical `v1` terminology remains, and maturity-roadmap wording remains absent.

31. §2.4 remained substantively unchanged.

32. §7.5 remained unchanged.

33. Known out-of-scope observation: §7.5 still contains “once the buffer layer exists”; it remains for the Chapter 7 review.

34. Files changed: `docs/ARCHITECTURE.md` only.

35. Changed-hunk classification:

   - §2.1 diagram description: B-M1.
   - §2.1 lower-layer rule: B-M2.
   - §2.5 diagram description: B-M1.
   - §2.5 BufferPool sentence: B-m1.
   - Remaining changes are line wrapping caused by those fixes.

36. `git diff --check`: **passed**.

37. Git state:

   - Initial: clean working tree, empty index, HEAD `fd1d837df9e85ebe1d611d725ddf66e60a9327a4`.
   - Final: only `docs/ARCHITECTURE.md` modified, index empty, HEAD unchanged.

38. `AGENTS.md`: unchanged.
39. `README.md`: unchanged.
40. `docs/PROJECT_STATE.md`: unchanged.
41. `docs/DEVELOPMENT.md`: unchanged.
42. `docs/VERIFICATION.md`: unchanged.
43. Chapter 1: unchanged.
44. Chapter 3+: unchanged, including §7.5.
45. Source, tests, benchmarks, and build configuration: unchanged.
