# Playable Candidate RPC ownership

| Boundary | Caller | Authority | Failure behavior |
| --- | --- | --- | --- |
| firearm shot consumption | ability graph spawn node | PlayerState on server | no projectile and explicit result log |
| firearm reload | native `R` or `hud_reload` | PlayerController RPC then PlayerState | result code; no client mutation |
| reward | death-policy dispatch | GameMode/server wallet | duplicate outcome ignored |
| purchase | authenticated owner session | Commerce world subsystem | explicit commerce result and rollback |
| tutorial completion | observed authoritative event | PlayerState server bitmask | client cannot mark complete |
| persistence | logout/checkpoint/shutdown | Persistence subsystem | prior verified generation retained |
