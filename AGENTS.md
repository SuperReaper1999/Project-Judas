# Project Judas branch workflow

## Development and checkpointing

- Use `Experimental` for ordinary development. Check the current branch and worktree before making changes; preserve unrelated local work.
- After appropriate automated validation, implementation tasks may be committed and pushed to `Experimental` without waiting for human testing or another checkpoint authorization. Report the commit hash, checks and remaining limitations.
- Explicit task instructions to leave work uncommitted, avoid pushing, or perform read-only work override this default permission.
- Do not claim human validation from automated tests, synthetic input, source inspection or a successful build.

## Main release promotion

- `main` is the human-tested release branch and requires explicit operator acceptance of the exact candidate. Never promote automatically from an Experimental push or an automated pass.
- Preserve a reviewed candidate commit when promoting; do not manufacture an untested intermediate state. Fast-forward `main` from the accepted Experimental checkpoint. If main has diverged, stop and establish the actual integration state before promotion.
- Push normally. No force pushes, history rewriting or milestone tags unless specifically authorized.
- The release lineage begins at `3102bf6c6e4b5c57eb3ab66a71fa502f8fc423f6`, expressly designated by the operator. After M82 acceptance, the operator retired the duplicate `Stable` branch and made `main` the release/default branch. This workflow change does not retroactively certify outstanding milestone, Windows or performance validation.

## Repository continuity

- The two primary branches are `main` for accepted releases and `Experimental` for development. Keep development on `Experimental`; do not update `main` automatically or promote unreviewed implementation through it.
- Preserve protected historical evidence, independent research and unrelated assets. Document actual failures and limitations rather than weakening fixtures to obtain a pass.
- GitHub protects main against force pushes and deletion, including administrator actions. Human-test acceptance is an operator workflow requirement; GitHub does not independently prove that testing occurred.
