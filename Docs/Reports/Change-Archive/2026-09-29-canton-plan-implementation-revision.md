# Canton plan revision against implemented baseline — 2026-09-29

## Intent

Revise the original twenty-day terrain plan against repository `1b2cc5d0`, the M01/M05 reviews, current scripts/contracts and the September 28 audit disposition. Keep the historical requirements and original work-package IDs while making remaining work actionable.

## Changed behavior

- The master plan now identifies reusable maps/raster paths, current milestone status, owners/open issues, remaining work per day and safe continuation order.
- All twenty daily plans carry the same baseline and remaining focus; the index explains that blank resource templates are not live execution status.
- Day 14 requires the actual pawn and resolved district-side threshold with closed doors preserved. Day 20 requires Development runtime/WP evidence; a blocked gate permits only a provisional handoff.
- Final material/paving/vegetation/debris work remains required. Historical H1/H2/Z and approval remain blocked/pending; no acceptance threshold was lowered.
- External source lead times cannot be guaranteed within the original twenty-day budget. Remaining effort must be re-estimated rather than restarting completed imports or silently reducing scope.

## Validation

Documentation consistency checks cover all twenty master/daily sections, local Markdown links, referenced issue IDs and SVG XML. Run `git diff --check`. Refresh the provisional → native → district manifests and repository freeze because existing snapshots include changed documentation. Run source-manifest, heightmap, UE-import, M01-readiness, district-contract, headless-runner and handoff-freeze checks. These verify the retained evidence and documentation snapshot; no new native UE, pawn, cook or runtime execution is claimed.

Results: all twenty daily sections match the master; all checked local Markdown links and issue IDs resolve; SVG XML parses and the rasterized diagram was visually inspected. Heightmap tests (2), UE-import tests (2), district contract and guarded-runner tests (6) pass. Source inventory reports `inventory_consistent: true` with no errors; M01 readiness retains technical provisional pass and blocked historical axes. The refreshed repository-freeze check and `git diff --check` pass.

The first source check exposed two pre-existing stale source-manifest hashes (acceptance budget and Day-01 resource sheet). Only those hashes were refreshed, preserving all 56 rows and their provenance/status. The generator currently lists fewer rows, so it was not used to discard the manually registered additions. Dependent native/district snapshots were regenerated. The source wrapper prints validation errors without propagating its validator return value; this review inspected `inventory_consistent` and the error list rather than relying on exit zero alone. No validator code was changed.

[Visual summary](2026-09-29-canton-plan-implementation-revision.svg)
