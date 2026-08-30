# Playable Candidate persistence contract

Player records own authenticated identity, role, wallet, inventory, completed firearm state, tutorial mask, and recovery state. World records own persistence namespace, generation, population, battle phase, merchant stock, restock timestamps, observed clock, and stock revision. In-flight reload state is never serialized. Save and checkpoint paths retain the prior verified generation on any player/world/manifest write or validation failure.
