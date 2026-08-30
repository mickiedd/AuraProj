# Playable Candidate life-state contract

The server owns `Alive -> Dying -> Dead -> Recovering -> Alive`. Death sequence dispatch is exactly once per victim sequence. Firearm reload timers are canceled on death, pawn replacement, controller disconnect, shutdown, and profile restore. The owner-only HUD reports recovery state and never grants client-side control during Dead or Recovering.
