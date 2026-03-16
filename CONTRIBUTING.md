# Contributing to Reflection

Welcome, and thank you for your interest in contributing to Reflection! Every contribution helps — whether it's a bug report, a feature idea, documentation improvement, or a code change.

## Getting Started

### Prerequisites

- macOS 13.0 (Ventura) or later
- Xcode 16+ with Swift 6.0
- An iPad and USB cable (for testing hardware features)

### Local Development

```bash
# Clone your fork
git clone https://github.com/<your-username>/reflection.git
cd reflection

# Build the project
swift build

# Run tests
swift test

# Create a .app bundle for manual testing
./scripts/make-app-bundle.sh
```

> **Note:** If `swift test` fails with "no such module 'XCTest'", run:
> ```bash
> export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
> ```

## How to Contribute

### Reporting Bugs

1. **Search [existing issues](https://github.com/SatanshuMishra/reflection/issues)** to check if it's already reported
2. **Open a new issue** with:
   - Steps to reproduce the bug
   - Expected vs. actual behavior
   - macOS version and iPad model
3. Use the **Bug Report** label if available

### Suggesting Features

1. **Open an issue** describing the feature and the problem it solves
2. Wait for maintainer feedback before starting work — this avoids wasted effort on changes that may not align with the project's direction

### Submitting Code

1. **Get approval first** — Every code change starts with an approved issue
2. **Fork the repository** and create a branch from `main`
3. **Make your changes** — Keep commits small and focused
4. **Write tests** — New functionality should include tests
5. **Run `swift test`** — All tests must pass before submitting
6. **Open a pull request** referencing the original issue

### Pull Request Guidelines

- Keep PRs focused on a single change
- Write a clear description of what changed and why
- Reference the related issue (e.g., "Closes #42")
- Ensure CI passes before requesting review

## Commit Convention

This project uses [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add wireless capture backend
fix: prevent crash when iPad disconnects during capture
refactor: extract frame monitoring into dedicated class
test: add frame stale monitor edge case coverage
docs: update roadmap with Linux support plans
```

## Code Review

All submissions require review by a maintainer. We aim to provide feedback within a few days. Once approved, a maintainer will merge your pull request.

## Community

- Be respectful and constructive — see our [Code of Conduct](CODE_OF_CONDUCT.md)
- If you find a security vulnerability, please report it privately — see [Security Policy](SECURITY.md)
