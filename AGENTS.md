# Project Judas branch workflow

## Development and checkpointing

- Use `Experimental` for ordinary development. Check the current branch and worktree before making changes; preserve unrelated local work.
- After appropriate automated validation, implementation tasks may be committed and pushed to `Experimental` without waiting for human testing or another checkpoint authorization. Report the commit hash, checks and remaining limitations.
- Explicit task instructions to leave work uncommitted, avoid pushing, or perform read-only work override this default permission.
- Do not claim human validation from automated tests, synthetic input, source inspection or a successful build.

## Stable promotion

- `Stable` requires explicit operator acceptance after human testing of the exact candidate. Never promote automatically from an Experimental push or an automated pass.
- Preserve a reviewed candidate commit when promoting; do not manufacture an untested intermediate state. If Stable has diverged, stop and establish the actual integration state before promotion.
- Push normally. No force pushes, history rewriting or milestone tags unless specifically authorized.
- Stable begins at `3102bf6c6e4b5c57eb3ab66a71fa502f8fc423f6`, expressly designated by the operator. This baseline choice does not retroactively certify outstanding milestone, Windows or performance validation.

## Repository continuity

- `main` is retained at the split checkpoint for existing links and history. New work goes to `Experimental`; do not update `main` automatically.
- Preserve protected historical evidence, independent research and unrelated assets. Document actual failures and limitations rather than weakening fixtures to obtain a pass.
- GitHub protects Stable against force pushes and deletion. Human-test acceptance is an operator workflow requirement; GitHub does not independently prove that testing occurred.
