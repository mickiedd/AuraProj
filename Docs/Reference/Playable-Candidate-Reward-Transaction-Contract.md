# Playable Candidate reward transaction contract

The GameMode consumes the authoritative death-policy event and issues the configured 25 gold CivilianLethal reward once per server correlation. `market_health_potion` atomically debits 25 gold, grants one health potion, and decrements finite stock. Any failed mutation restores wallet, inventory, and stock revisions before returning a failure code.
