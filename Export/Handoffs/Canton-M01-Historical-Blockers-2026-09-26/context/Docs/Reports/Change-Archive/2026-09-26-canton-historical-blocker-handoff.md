# Canton historical-blocker handoff — 2026-09-26

Intent: export a self-contained packet that another agent can use to resolve the remaining Canton M01 historical evidence gates.

Before: the source evidence, failed georeference, vertical-datum notes, plans and validators were distributed across the repository. A receiving agent would need repository access and substantial reconstruction before starting useful research.

After: `Export/Handoffs/Canton-M01-Historical-Blockers-2026-09-26/` contains the permitted historic map and modern elevation context, failed transform and residuals, source register, Days 01–10 evidence, complete 20-day plan set, relevant scripts, exact return schemas, an agent-ready task brief, and a fail-closed packet validator. Publisher PDFs whose redistribution rights are unclear remain stable source links with registered checksums.

The packet preserves the central guard: its supplied horizontal and vertical baselines are `Blocked`. The validator rejects promotion of the failed transform, population of the historical terrain zero, or promotion of the registered Qing cultural-layer candidate. The receiving agent must add new versioned evidence rather than overwrite the supplied failure record.

Validation: the packet's per-file `FILE_INDEX.csv` and `MANIFEST.sha256` agree; `python3 tools/validate_packet.py` passes; the compressed archive is tested and has a companion SHA-256 file. The SVG was rendered and visually inspected.

[Visual summary](2026-09-26-canton-historical-blocker-handoff.svg) · [Packet README](../../../Export/Handoffs/Canton-M01-Historical-Blockers-2026-09-26/README.md)
