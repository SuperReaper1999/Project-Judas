# Editor milestone notification — Windows follow-up

Native Windows 10 VM verification, 2026-10-11. Release editor built with MSVC;
the eight transferred source files matched their recorded SHA-256 hashes.
Existing MSVC warnings were retained. The VM uses its existing app-local Mesa
software renderer; physical Windows GPU/input validation is not implied.

| Environment | Startup | Manual View / Check for new milestones | Shutdown |
|---|---|---|---|
| GitHub CLI absent from executable search paths | Responsive; no prompt/notice | Responsive; “Milestone check unavailable” status | Exit 0 |
| Official portable GitHub CLI 2.102.0, empty login configuration, token variables removed | Responsive; no prompt/notice | Responsive; “Milestone check unavailable” status | Exit 0 |

CLI authentication independently returned exit 1 (not signed in); the API command
returned exit 4. The editor suppressed CLI diagnostics as intended. UI checks used
ordinary desktop sessions, actual mouse input, screenshots and a bounded WM_NULL
responsiveness probe. No runtime/source correction was needed.

Receipts: [build/source hashes and final results](windows-results.json). Screenshots,
logs and temporary tooling remain locally in `.cache/windows-vm-transfer/`.
The initial missing-CLI manual probe accidentally clicked its foreground PowerShell
window and did not retain an exit handle. That receipt was preserved and excluded;
the corrected probe clicked the editor menu and retained a verified exit code.

No production suites were rerun; no commit or push was performed.
