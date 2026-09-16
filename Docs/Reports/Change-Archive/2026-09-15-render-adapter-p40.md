# Render adapter — switch the editor back to the Tesla P40

- **Date:** 2026-09-15
- **Illustration:** [2026-09-15-render-adapter-p40.svg](2026-09-15-render-adapter-p40.svg)
- **Scope:** workstation render configuration + `Scripts/PreflightRenderAdapter.ps1` (new)

## Intent

The machine has two graphics adapters:

| Adapter | Role | Dedicated VRAM | Display outputs |
| --- | --- | --- | --- |
| Intel(R) Iris(R) Xe Graphics | integrated, drives the 1920×1080 panel | 128 MB | 2 |
| NVIDIA Tesla P40 | render/compute | 24268 MB | 0 |

The request was to switch Unreal's rendering to the P40. Investigation showed the
editor had **already been rendering on the P40** up to the 2026-09-14 07:35 session, and
had silently fallen back to the Intel iGPU for the session that started at 00:18 today.

## Root cause

Unreal Engine 5.5 picks its D3D12 adapter with
`IDXGIFactory6::EnumAdapterByGpuPreference(..., DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE)`
(`Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Device.cpp`). With no
override it then prefers `BestMemoryAdapter` — the non-integrated, non-WARP adapter with
the largest dedicated video memory. On this machine that is always the P40.

The machine rebooted at 00:15 today and the editor was started at 00:18:32, roughly three
minutes after power-on. At that moment the P40 was **not present in the enumeration at
all**, so UE saw only Intel and WARP and selected Intel. The consequence was not merely a
slower GPU:

- the Intel iGPU reports `AtomicInt64OnTypedResourceSupported = false`;
- UE requires that capability for its D3D12/SM6 RHI path, so it computed SM5 for Intel;
- D3D12/SM6 was rejected and the **entire engine fell back to D3D11/SM5**;
- **Nanite and Lumen were disabled** for the whole session —
  `UseNanite(PCD3D_SM5) returned false` appears **166 times** in that log.

The fallback is completely silent: the editor starts normally and looks fine.

## Changed behaviour

| | Before (00:18 session) | After (08:58 session) |
| --- | --- | --- |
| D3D12 adapters seen | `[0] Intel Iris Xe`, `[1] WARP` | `[0] Tesla P40`, `[1] Intel Iris Xe`, `[2] WARP` |
| `Chosen D3D12 Adapter Id` | 0 → Intel Iris Xe | 0 → **NVIDIA Tesla P40** |
| RHI actually used | D3D11 / SM5 | **D3D12 / SM6** |
| Nanite | disabled (166 messages) | enabled (0 messages) |
| Lumen | disabled | enabled |

No engine or project setting was changed. The engine's default selection rule was already
correct; it only needed the P40 to be enumerable, which it now is.

Deliberately **not** done: pinning `-graphicsadapter=0` or `r.GraphicsAdapter=0`. The
adapter index is an index into the GPU-preference-ordered list, so if the P40 were absent
again index 0 would be Intel — the pin would not prevent the failure and would only add a
knob that does nothing.

## Guard

`Scripts/PreflightRenderAdapter.ps1` (new) detects the silent degradation.

- `-Preflight` (default) walks the `IDXGIFactory6` and `ID3D12Device` vtables directly and
  reproduces UE's enumeration, then fails if the P40 is missing, if `D3D12CreateDevice`
  fails on it, or if it reports `AtomicInt64OnTypedResourceSupported = false`.
- `-Verify` reads `Saved\Logs\Aura.log` and asserts, using UE's own log verdict, that the
  session got the P40 with D3D12/SM6 and logged no Nanite-disabled messages.

Vtable slots were taken from the Windows SDK 10.0.22621.0 headers and counted
programmatically rather than assumed: `EnumAdapterByGpuPreference` is slot 29 of
`IDXGIFactory6`, and `CheckFeatureSupport` is slot 13 of `ID3D12Device`.

## Tests

| Test | Result |
| --- | --- |
| `-Preflight` on the current machine | PASS, exit 0 — P40 at index 0, 23.7 GB, D3D12 OK, atomic64 true |
| `-Verify` against the live session log | PASS, exit 0 — chose P40, D3D12/SM6 in use, no Nanite-disabled messages |
| `-Verify` against `Aura-backup-2026.09.15-00.46.34.log` (negative test) | FAIL, exit 1 — chose Intel, D3D11/SM5 fallback, 166 Nanite-disabled messages |
| Editor relaunch end-to-end | `Chosen D3D12 Adapter Id = 0` + `RHI D3D12 with Feature Level SM6 is supported and will be used` |

## Related finding — earlier visual QA is confounded

Every V4 landmark capture made between 01:04 and 07:15 today was rendered by the 00:18
session, i.e. with **Nanite and Lumen disabled**. The "roofs read black and the walls read
as flat slabs" conclusion recorded in
`2026-09-15-v4-reference-conformance-review.md` is exactly the symptom of missing Lumen GI
and Nanite, so that review's material-tint diagnosis is confounded by the adapter failure
and should be re-checked against fresh captures taken on the P40.

## Separate unresolved issue

`nvidia-smi` fails with `Failed to initialize NVML: Unknown Error` even though the card is
healthy for D3D12. `nvml.dll` and `nvidia-smi.exe` are both present in `System32` at
version `8.17.15.3867`, matching the installed driver `31.0.15.3867`, so this is not a
version mismatch. The installed package is the **vGPU/GRID-class driver** — it is bound
from `nv_dispwi.inf` (whose copy-file sections are `nv_*_VGPU_copyfiles`), the adapter key
carries `GridLicensedFeatures = 7`, and there is no `GridLicensing` key or `nvidia-gridd`
service. NVML in the vGPU driver line expects an active vGPU licence, which a bare-metal
P40 does not have. Replacing it with the standard NVIDIA Data Center / Tesla graphics
driver would be the correct fix and is not yet done.
