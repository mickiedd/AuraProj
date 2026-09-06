# HUD icon crash fix

The captured client crash dereferenced an invalid `UTexture2D` in `AAuraHUD::BuildAbilityIconDataUri`, with adjacent `iconName` serialization using the same unsafe pointer check.

Changed behavior: all three HUD texture dereferences require `IsValid`; invalid or pending-kill textures now produce an empty icon value instead of crashing the client.

Validation: checked the crash context and call stack, scanned the UnrealBuildTool log for compiler/linker errors, and verified the guarded source call sites. Illustration: [hud icon crash guard](2026-09-06-hud-icon-crash-fix.svg).
