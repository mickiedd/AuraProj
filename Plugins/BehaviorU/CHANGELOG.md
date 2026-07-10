# BehaviorUPlugin - Recent Updates (2026-02-25)

## 🎯 New Features

### 1. Debug Logging System
Track exactly what's happening during XML import and property parsing:

**Runtime Debug Logs:**
- `📦 Action::LoadFromProperties: Got X properties` - Property count
- `📦 Property: 'Name' = 'Value'` - Each property parsed
- `📦 After parsing: MethodName='...'` - Final result

**XML Parser Debug Logs:**
- `📋 XML Parser: Found property 'Name'='Value'` - XML attribute extraction
- `📋 ParseNodeFromXML: Node has X properties` - Property count per node

**Usage:**
Enable these logs in UE5 Output Log to diagnose:
- Empty Method names
- Missing properties
- XML parsing issues

### 2. Native Reimport Support
No more manual cache deletion! Right-click → Reimport.

**How It Works:**
- Implements UE5's `FReimportHandler` interface
- Stores `SourceFilePath` in behavior tree asset
- Automatically reloads from XML when "Reimport" is clicked
- No editor restart needed

**Before:**
```
1. Edit XML
2. Delete .uasset manually
3. Restart editor
4. Test
```

**After:**
```
1. Edit XML
2. Right-click asset → Reimport
3. Test ✅
```

### 3. Editor Console Commands
Run commands directly in UE5 Output Log:

```
BehaviorU.ReimportBT BT_SimpleNPC
BehaviorU.ReimportAllBT
BehaviorU.DeleteBTCache BT_SimpleNPC
```

Useful for:
- Automation scripts
- Batch operations
- CI/CD pipelines
- Power users

## 🔧 Technical Implementation

### Files Modified:

**BehaviorUActions.cpp** (+14 lines)
- Added debug logging in `LoadFromProperties()`
- Logs property count, name/value pairs, final MethodName
- Uses 📦 emoji prefix for easy filtering

**BehaviorUBehaviorTree.cpp** (+14 lines)
- Added debug logging in `ParseNodeFromXML()`
- Traces XML attribute → property conversion
- Uses 📋 emoji prefix for easy filtering

**BehaviorUBehaviorTreeFactory.h** (+13 lines)
- Added `#include "EditorReimportHandler.h"`
- Inherited from `FReimportHandler` interface
- Declared reimport interface methods

**BehaviorUBehaviorTreeFactory.cpp** (+75 lines)
- Implemented `CanReimport()` - Checks if asset has source path
- Implemented `SetReimportPaths()` - Updates source path
- Implemented `Reimport()` - Reloads from XML and updates asset
- Improved error messages with emoji indicators

**BehaviorUEditorModule.cpp** (+1 line)
- Added `#include "BehaviorUEditorCommands.h"`
- Enables console commands on module load

**BehaviorUEditorCommands.h** (NEW FILE, +90 lines)
- Defines `FAutoConsoleCommand` instances
- Three commands: ReimportBT, ReimportAllBT, DeleteBTCache
- Editor-only (wrapped in `#if WITH_EDITOR`)

## 📊 Statistics

- **Total Changes:** +207 insertions, -7 deletions
- **Files Modified:** 5
- **Files Added:** 1
- **Commit Hash:** `5e38bdd`

## 🧪 Testing

Tested on:
- **Engine:** Unreal Engine 5.5
- **Platform:** macOS (Apple M4)
- **Project:** TopDownBehaviorUTest

**Test Cases:**
1. ✅ XML import creates .uasset
2. ✅ Right-click Reimport updates from XML
3. ✅ Debug logs show property parsing
4. ✅ Console commands work in editor
5. ✅ Empty MethodName issue diagnosed

## 🚀 Usage Examples

### Example 1: Debug Empty Method Names

**Problem:** Action nodes executing with empty `MethodName`

**Solution:**
1. Enable debug logs in Output Log
2. Look for: `📦 After parsing: MethodName=''`
3. Check: `📋 XML Parser: Found property...` to verify XML is correct
4. If properties are missing → XML parsing bug
5. If properties present but MethodName empty → LoadFromProperties bug

### Example 2: Quick XML Iteration

**Workflow:**
```bash
# 1. Edit XML
vim Content/AI/BT_SimpleNPC.xml

# 2. Reimport (choose one):

# Option A: In Editor
# Right-click asset → Reimport

# Option B: Console
BehaviorU.ReimportBT BT_SimpleNPC

# Option C: Script (if editor closed)
./Scripts/reimport_bt.sh BT_SimpleNPC

# 3. Test
# Play In Editor (PIE)
```

### Example 3: Batch Update All Trees

**Scenario:** Updated XML schema, need to reimport all BTs

```
# In UE5 Editor Output Log:
BehaviorU.ReimportAllBT
```

Or via script:
```bash
cd /path/to/project
./Scripts/clear_bt_cache.sh  # Clear all
# Restart editor - auto-reimports all
```

## 🔗 Related Tools

**Shell Scripts** (in test project):
- `Scripts/reimport_bt.sh` - Smart reimport helper
- `Scripts/clear_bt_cache.sh` - Batch cache cleaner
- `Scripts/README.md` - Full documentation

These scripts complement the plugin features and provide CLI workflows.

## 📝 Migration Notes

If you're updating an existing project:

1. **Rebuild Plugin:**
   ```bash
   cd /path/to/project
   rm -rf Plugins/BehaviorUPlugin/Binaries
   rm -rf Plugins/BehaviorUPlugin/Intermediate
   # Rebuild via Xcode or Build.sh
   ```

2. **Restart Editor:**
   - Loads new console commands
   - Enables reimport button

3. **Test Reimport:**
   - Right-click any BT asset
   - "Reimport" should appear in menu
   - Select XML file to test

## 🐛 Known Issues

None currently. Debug logs should help identify any future issues quickly.

## 📚 Further Reading

- **UE5 FReimportHandler:** [Docs](https://docs.unrealengine.com/5.3/en-US/API/Editor/UnrealEd/Factories/FReimportHandler/)
- **Console Commands:** [Guide](https://docs.unrealengine.com/5.3/en-US/console-commands-in-unreal-engine/)
- **Asset Import API:** [Reference](https://docs.unrealengine.com/5.3/en-US/API/Editor/UnrealEd/Factories/UFactory/)

---

**Commit:** `5e38bdd` | **Date:** 2026-02-25 | **Author:** AI Doggy 🐶
