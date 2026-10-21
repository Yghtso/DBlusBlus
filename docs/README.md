# DBlusBlus documentation

DBlusBlus is a learning-oriented, single-node relational database written in C++20.

| Document | What it is for |
|---|---|
| [Architecture](ARCHITECTURE.md) | The technical contract: intended behavior, formats and invariants. It is the authority when documents disagree. |
| [Project state](PROJECT_STATE.md) | What is implemented today, its limits, and known mismatches with the architecture. |
| [Development guide](DEVELOPMENT.md) | Toolchain, build presets, implementation order and code conventions. |
| [Verification](VERIFICATION.md) | How each architectural obligation is tested and benchmarked. |

Section numbers such as `§4.13.3` always refer to the architecture. In the rendered book each
one links to its section.

## Building this book

The book is built with [mdBook](https://rust-lang.github.io/mdBook/) 0.4.40 and Python 3:

```sh
mdbook build docs          # writes build/book/
mdbook serve docs --open   # live preview
python3 docs/tools/check_links.py build/book
```

The Markdown files also read normally on GitHub; the book adds navigation, search and
section links.
