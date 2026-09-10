#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAura, Log, All);

// Diagnostic animation probes are opt-in. Enable with -LogCmds="LogAuraAnimationDiagnostics Verbose".
DECLARE_LOG_CATEGORY_EXTERN(LogAuraAnimationDiagnostics, Log, All);
