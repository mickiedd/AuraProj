# Playable Candidate economy contract

The server issues one `RoleBattle.CivilianLethalReward` of 25 gold per authoritative Civilian lethal outcome correlation. The canonical purchase is `market_health_potion`: 25 gold for one `health_potion`. Wallet debit, item grant, and finite stock decrement commit atomically; any failed commit rolls back every component and reports an explicit result code.

The finite health-potion offer starts at 20. Restock uses server UTC with `LastObservedAtUtc = max(previous, observed)`, an inclusive 600-second boundary, and at most one startup catch-up decision. Backward clocks cannot create extra stock, and offline time never loops through multiple missed intervals. Restock timestamps and stock revision are part of the world record.
