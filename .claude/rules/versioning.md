# Versioning Policy (Reflection)

Follows [Semantic Versioning 2.0.0](https://semver.org/). Format: `MAJOR.MINOR.PATCH`

## When to Bump

| Level | Trigger | Examples |
|-------|---------|----------|
| **MAJOR** | Breaking changes | `ScreenCapture` protocol signature change, minimum macOS version bump, architectural restructuring |
| **MINOR** | New features (backwards compatible) | New capture backend, new UI component, new device type support |
| **PATCH** | Bug fixes, non-feature changes | Bug fixes, performance improvements, refactoring, documentation, CI updates, test additions |

## Commit Type → Version Impact

- `feat!` or `BREAKING CHANGE` footer → **MAJOR**
- `feat` → at minimum **MINOR**
- `fix`, `refactor`, `perf`, `docs`, `test`, `ci`, `chore` → **PATCH**

## Release Process

After completing a set of changes:

1. Determine version bump from commit types since last tag
2. Update `CFBundleShortVersionString` in `SupportingFiles/Info.plist`
3. Commit: `chore: bump version to X.Y.Z`
4. Create annotated tag: `git tag -a vX.Y.Z -m "vX.Y.Z: brief summary"`
5. Push: `git push origin main && git push origin vX.Y.Z`
6. Release workflow auto-creates GitHub Release with DMG

## Rules

- Never skip versions — always increment from the current latest tag
- Always use annotated tags (`-a`), not lightweight tags
- Tag message should summarize the release in one line
- Current version: check with `git describe --tags --abbrev=0`
