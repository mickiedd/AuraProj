# Playable Candidate server operations

This runbook is the bounded local procedure for the Days 21-40 candidate. A second developer starts from a clean checkout, runs the fast contract gate, and only then attempts packaged profiles.

```powershell
Set-Location C:\Git\AuraProj
powershell -NoProfile -ExecutionPolicy Bypass -File .\RunPlayableCandidate.ps1 -Stage Fast -RunId dev2-fast
```

The fast gate validates frozen scope, discovered JSON/XML/WebUI content, the four lane declarations, firearm/reload semantics, reward and merchant atomicity, persistence fields, reconnect generation, redacted diagnostics, and bounded soak bookkeeping. Each Day 21-40 wrapper emits its named JSON artifact under `Saved/Reports/PlayableCandidate/working/<RunId>/`.

For a packaged attempt, provide built listen and dedicated executables/profile roots to the second developer and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RunPlayableCandidate.ps1 -Stage Candidate -Mode Both -Soak -RunId dev2-candidate
```

The candidate stage is `BLOCKED` when packaged evidence is absent; that is an honest gate result and does not convert local contract PASS into a packaged PASS. External identity/provider validation remains `BLOCKED` until production credentials and two authorized stable identities are provisioned. Never put provider secrets, raw external IDs, or connection strings into an evidence artifact.

Recovery procedure: stop the run, preserve the working artifact directory and logs, fix the first failing contract, and rerun with a new `RunId`. Do not overwrite an existing candidate artifact. A final `candidate.json` is written once only after all four lane records, restart/reconnect/forced-kill evidence, cleanup evidence, and SHA-256 inputs are present.
