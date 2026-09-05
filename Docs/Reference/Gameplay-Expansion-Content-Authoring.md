# Gameplay Expansion Content Authoring

This workflow composes encounters from existing gameplay verbs. It does not authorize new C++ types or runtime publication.

## Training encounter recipe

1. Add an encounter row named `training_clear` using the existing `Raider` archetype and `Evade` counter verb.
2. Keep the row budget at or below the manifest budget and reference only IDs already listed by the gameplay definitions.
3. Run `python3 Scripts/validate_gameplay_expansion_content.py --repo-root . --manifest Content/Config/GameplayExpansionManifest.json`.
4. Stage into a separate directory with `--staged-root <path>`; publication is accepted only when every staged byte has the manifest SHA-256.
5. Run `Aura.Gameplay.Day57` and the relevant packaged lane. Validation failure leaves the current registry generation unchanged.

The legacy `PlayableCandidateManifest.json` is read-only and belongs to a separate profile. A second author must still complete the packaged walkthrough before Day 57 can be signed off.
