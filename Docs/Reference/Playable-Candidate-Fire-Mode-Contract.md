# Playable Candidate minimum fire-mode contract

FireGun is `SemiAuto` with a 0.2 second server minimum interval and one round consumed per accepted press. A held or reordered duplicate request does not create another acceptance; rejected requests leave ammo, damage, and cooldown state unchanged. Browser disconnect and the two-second input lease release the held input path.
