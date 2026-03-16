# Git Commit Rules (Reflection)

## Format

```
type: concise description

Optional body explaining why, not what.
```

## Valid Types

`feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`

## Rules

- **No Claude attribution.** Never add `Co-Authored-By` lines or mention Claude/AI in commits.
- **Authored by repository owner only.** Do not modify git author config.
- **One logical change per commit.** Keep commits small and incremental.
- **Lowercase description.** No period at the end. Max 72 characters.
- **Body for non-trivial changes.** Explain the "why" — the "what" is in the diff.
- **Reference issues when applicable.** Example: `fix: handle nil capture session on disconnect (#42)`
- **Never amend published commits.** Create new commits to fix mistakes.

## Automatic Version Bumping

**After committing changes, always bump the version.** Follow `versioning.md` for the full policy. Summary:

1. Determine the highest-impact commit type since the last tag:
   - `feat!` / `BREAKING CHANGE` → **MAJOR**
   - `feat` → **MINOR**
   - Everything else (`fix`, `refactor`, `perf`, `docs`, `test`, `ci`, `chore`) → **PATCH**
2. Get current version: `git describe --tags --abbrev=0`
3. Increment the appropriate component (reset lower components to 0)
4. Update `CFBundleShortVersionString` in `SupportingFiles/Info.plist`
5. Commit: `chore: bump version to X.Y.Z`
6. Create annotated tag: `git tag -a vX.Y.Z -m "vX.Y.Z: brief summary"`
7. Push **both** commits and tag: `git push origin main && git push origin vX.Y.Z`
   - Tags are NOT pushed by `git push` alone — the tag push triggers the release workflow

**Batching:** Multiple commits may share a single version bump. When committing a batch of related changes, make all the code commits first, then do one version bump at the end using the highest-impact commit type from the batch.

**When to skip:** Do not bump for the version bump commit itself (`chore: bump version to X.Y.Z`). Only bump once per logical batch of changes.

## Examples

```
feat: add wireless capture backend
fix: prevent crash when iPad disconnects during capture
refactor: extract frame monitoring into dedicated class
ci: update Xcode to 16.2 for Swift 6.0 compatibility
docs: add CLAUDE.md with project documentation
test: add frame stale monitor edge case coverage
```
