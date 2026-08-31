# Gameplay expansion plans — Days 41–60

Date: 2026-08-31. Change type: documentation and planning only.

## Intent

Continue the existing development sequence with implementation-grade gameplay plans: make combat decisions, objectives, cooperation and replay the center of the next milestone, while optimizing the architecture at the specific ownership/lifetime seams needed to support them.

## Before and after

Before: Days 21–40 describe the launch/combat/economy/recovery candidate, with packaged evidence still blocked in the checked-in review. Source offers two roles, enemy/civilian systems and persistence, but no bounded gameplay-expansion contract.

After this planning change: a master roadmap, primary-source comparison analysis, architecture contract, shared execution contract and twenty individual Days 41–60 plans specify three mission templates, four enemy archetypes, eight run augments, two layouts, two optional mutators, rescue/supplies and a captain finale. The plan separates mission state, encounter/AI ownership, combat authorization and durable settlement. Existing code behavior is unchanged.

Important guards: Day 40 local packaged evidence gates runtime entry; one AI/spawn owner per enemy; run resources/effects cannot leak into profiles; shared rewards commit once; missing human/performance/provider proof cannot be reported as passed. Earlier candidate scope and archive records remain intact.

## Validation

Twenty daily contracts and 117 named future test cases inventoried; daily headings/steps/dependencies, relative links, named existing paths, SVG XML, offline rendering and documentation whitespace/scope checked. The SVG was rendered at 1440×1250 with installed Sharp and visually inspected. Browser file navigation was blocked by policy; no browser workaround or external transmission was used.

No runtime build, packaged multiplayer, gameplay test, human playtest or external independent review is claimed for this documentation-only task. See the [planning review](../Gameplay-Expansion-Plan-Review-2026-08-31.md) for resolved findings and execution risks.

## Links

- [Master roadmap](../../Plans/Gameplay-Expansion-Implementation-Plan-2026-08-31.md)
- [Game analysis](../../Plans/Gameplay-Expansion-Game-Analysis-2026-08-31.md)
- [Architecture](../../Plans/Gameplay-Expansion-Architecture-2026-08-31.md)
- [Before/after illustration](2026-08-31-gameplay-expansion-days-41-60.svg)
