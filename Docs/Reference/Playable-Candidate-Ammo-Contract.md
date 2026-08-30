# Playable Candidate firearm contract

`FireGun` is an XML data ability with a server-owned `FAuraFirearmState` ledger on `AAuraPlayerState`. BungeeMan starts with a 12-round magazine and 48 reserve rounds. One LMB press can produce at most one accepted shot; held callbacks are ignored for this ability. A server cadence check of 0.2 seconds and a server-side decrement protect the authoritative result.

Reload is requested by the native `R` key or WebUI `hud_reload` command. The request crosses `ServerRequestFirearmReload`, validates role, life, capacity, reserve, and existing reload state, then completes after 1.25 seconds. Reload is canceled on death, pawn replacement, controller end play, and restore. Only completed magazine/reserve values are persisted; a reload in flight is never serialized.

Aura receives an owner-only `NotApplicable` state and does not receive firearm ammunition. Every state payload includes `ammoRevision`, `reloadSerial`, and an unavailable reason for deterministic HUD replay.
