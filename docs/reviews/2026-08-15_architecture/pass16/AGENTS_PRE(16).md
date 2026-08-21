# Personal Codex Instructions

## Working style

Before making non-trivial changes, inspect the existing code first.

Prefer small, incremental changes over large rewrites.

Do not rewrite working code just to make it look different unless specified.

When the task is ambiguous, state the assumption you are making before editing.

Preserve the existing project structure, naming style, and formatting conventions unless there is a clear reason to change them.

## Safety

Do not delete files, reset Git state, rewrite history, or discard uncommitted changes unless I explicitly ask.

Do not run destructive commands such as `rm -rf`, `git reset --hard`, or `git clean -fd` unless I explicitly approve them.

Do not install system packages unless I explicitly approve.

Do not add new project dependencies without explaining why they are needed.

## Git

Before making substantial edits, check the current Git status.

Do not commit changes unless I explicitly ask.

After making changes, summarize the modified files and the reason for each change.

## Testing

When changing code, run the smallest relevant test/check first.

If that passes, run the broader project checks when reasonable.

Do not claim a change works unless the relevant checks were run.

If tests fail, explain whether the failure appears related to the change.

## Code quality

Prefer clear, boring, maintainable code.

Avoid cleverness unless it is justified by performance, correctness, or API design.

Use meaningful names.

Keep functions focused.

Avoid unnecessary dependencies.

Prefer explicit error handling over panics in production/library code.
