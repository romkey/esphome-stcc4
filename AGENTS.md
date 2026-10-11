# Instructions for AI assistants

Follow https://github.com/romkey/esphome-ai-coding-rules/blob/main/RULES.md.
With Claude Code, the `esphome-rules` plugin loads them for you.

Read docs/overview.md first. `main` is protected: work on a branch and
open a pull request; CI and Lint must pass before it can merge.

<!-- Project-specific notes go below. -->

- Timings, commands and register values in `components/stcc4/` come from
  `datasheets/STCC4.pdf`; cite the section when changing them.
- `examples/` together must use every component option, so CI compiles all
  of them.
