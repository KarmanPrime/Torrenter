# Policy and Guidelines for AI Operations

## Project Background

Read [README.md] first if you need a project overview. \
For BitTorrent protocol, consult [Wikipedia article] for a high-level overview and [bittorrent.org] for protocol specification.

When opening an issue or a pull request, check [CONTRIBUTING.md] beforehand. \
Additionally, consult the README and CODING_GUIDELINES files in the respective folders before working within them.

[bittorrent.org]: https://www.bittorrent.org/beps/bep_0000.html
[CONTRIBUTING.md]: CONTRIBUTING.md
[README.md]: README.md
[Wikipedia article]: https://en.wikipedia.org/wiki/BitTorrent

## Communication

Use clear, appropriate English for non-native readers.

## Code Review Guidelines

* Respect project configuration. \
  Avoid reading excluded files unless it is strictly necessary to complete the task. Do not review or suggest changes for files/directories excluded in:
  * .editorconfig
  * .gitignore
  * .pre-commit-config.yaml
* Leave Qt translation (.ts) files alone. \
  Do not add or update strings there, as this is handled separately.
* Prioritize review metrics in this order:
  * Correctness
    * Logic errors and edge cases
    * Memory leaks and unsafe memory access
    * Security vulnerabilities
    * API misuse
    * Incorrect error handling
  * Performance
  * Coding style
    * Follow the rules defined in [CODING_GUIDELINES.md]
    * Prefer idiomatic expressions
    * Check for English grammar issues
* For each issue found, explain impact clearly and provide concrete, actionable fixes.

[CODING_GUIDELINES.md]: CODING_GUIDELINES.md

## Torrenter commit policy

This is the Torrenter fork of qBittorrent. After every change, build into `build-auto`. If the build passes, `git add -A`, commit with a short imperative message describing the change, and `git push origin master` to https://github.com/KarmanPrime/Torrenter. If the build fails, do not commit; fix the build or report the failure.

Never force-push, never rewrite history, and never commit secrets or files outside the repo.

Upstream qBittorrent PRs/issues still must be human-submitted. Do not create or submit issues, pull requests, or any engagement to the qbittorrent/qBittorrent community on behalf of the user.

## Document Purpose

This document provides policy and guidelines for AI operations. \
Do not expect this file to contain detailed instructions for compilation and testing.
