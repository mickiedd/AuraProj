# Day 18 - Add Persistence

## Goal

Persist the player role, wallet, inventory, and the minimum population state needed by the first vertical slice.

## Files to modify

- Source/Aura/Public/Save/LoadScreenSaveGame.h
- Source/Aura/Private/Save/LoadScreenSaveGame.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Player/AuraPlayerState.cpp
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp

## Implementation steps

1. Add a save-data version for the new fields.
2. Add wallet balance and inventory entries to player save data.
3. Preserve existing role, level, XP, attributes, and ability data.
4. Add migration defaults for old saves with no economy fields.
5. Save role through the existing validated role ID, not raw asset paths.
6. Save population state only if the game requires deaths to persist across reload:
   - Population ID.
   - Member ID.
   - Alive/dead state.
   - Respawn time or next refill state.
7. Keep transient AI controller state out of the save.
8. Ensure save/load runs only on the authority.
9. Test save after purchase, after death, after respawn, and after role selection.
10. Test loading an old save created before the new fields existed.

## Verification

- Existing saves still load.
- Aura and BungeeMan roles restore correctly.
- Wallet and inventory restore correctly.
- A civilian population does not duplicate after world load.
- In-progress or invalid transactions are never saved as completed.

## Completion gate

Progression and economy survive save/reload without breaking existing saves or duplicating population members.

