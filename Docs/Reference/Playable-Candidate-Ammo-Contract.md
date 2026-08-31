# Playable Candidate firearm contract

`FireGun` is an XML data ability with a server-owned `FAuraFirearmState` ledger on `AAuraPlayerState`. BungeeMan starts with a 12-round magazine and 48 reserve rounds. One LMB press can produce at most one accepted shot; held callbacks are ignored for this ability. A server cadence check of 0.2 seconds and a server-side decrement protect the authoritative result.

Reload is requested by the native `R` key or WebUI `hud_reload` command. The request crosses `ServerRequestFirearmReload`, validates role, life, capacity, reserve, and existing reload state, then completes after 1.25 seconds. Reload is canceled on death, pawn replacement, controller end play, and restore. Only completed magazine/reserve values are persisted; a reload in flight is never serialized.

Aura receives an owner-only `NotApplicable` state and does not receive firearm ammunition. Every state payload includes `ammoRevision`, `reloadSerial`, and an unavailable reason for deterministic HUD replay.

## FireGun owner HUD

The ammunition card and LMB ammo badge apply only when the payload identifies `BungeeMan`, `Abilities.Gun.Fire`, and `InputTag.LMB`, with `applicable=true`. The skill badge additionally requires that LMB actually contains FireGun; Aura/FireBolt and all other slots keep their existing presentation.

- Ready: magazine/capacity and reserve count; partial magazines with reserve permit `Reload [R]`.
- Empty magazine with reserve: `Magazine empty`, `Press R or click Reload to fire again`, and an R badge on FireGun.
- Reloading: explicit status on both panels, configured duration (1.25 seconds), and disabled reload requests. No client countdown completes or refills ammunition; the server's next state determines the result.
- Empty magazine without reserve: `Out of ammo`, with reload disabled, distinct from a reloadable empty magazine.
- Dead/recovering: unavailable, with firing/reload availability false. Life/role transitions refresh availability even without an ammo revision.
- Disconnect: clear stale gun presentation until a fresh state arrives. `hud_ready` replays current owner state on reconnect.

Native R and the WebUI reload button still use the existing authoritative request. R also works inside the right-top/bottom browser panels; repeated/modified keys, editable controls, and the right-top open menus do not trigger a reload. The right-top host remains 366 by 210 pixels; no larger mouse-capturing surface is introduced. Ammo updates modify existing skill nodes in place to preserve pointer-release behavior.

Verification: `Aura.UI.WebHUD.FirearmPresentation` tests the production native payload helper. `node Scripts/firearm_hud_fixture.mjs --check` parses the actual shipped scripts. Run `node Scripts/firearm_hud_fixture.mjs`, open `http://127.0.0.1:8766`, and click **Run regression tests** for real-DOM state/input/layout tests with a substituted WebSocket transport. This fixture is not a live multiplayer gameplay test.
