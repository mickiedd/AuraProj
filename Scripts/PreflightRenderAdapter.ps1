<#
.SYNOPSIS
    Preflight and verification for the Unreal render adapter on the Aura workstation.

.DESCRIPTION
    This machine has two GPUs:

        Intel(R) Iris(R) Xe Graphics  - integrated, drives the display, 128 MB dedicated
        NVIDIA Tesla P40              - 24268 MB dedicated, NO display outputs

    Unreal Engine 5.5 chooses its D3D12 adapter with
    IDXGIFactory6::EnumAdapterByGpuPreference(..., DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE)
    (Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Device.cpp).
    With no override it then prefers the non-integrated adapter with the largest
    dedicated video memory, so it picks the P40.

    The failure mode this script guards against is SILENT:

        P40 not enumerated  ->  UE picks the Intel iGPU
        Intel reports AtomicInt64OnTypedResourceSupported = false
                            ->  UE computes max RHI feature level SM5
                            ->  D3D12/SM6 is rejected
                            ->  the whole engine falls back to D3D11/SM5
                            ->  Nanite and Lumen are disabled

    That happened on the 2026-09-15 00:18 boot, when the P40 was not yet enumerable
    ~3 minutes after power-on. The editor came up on D3D11/SM5 and every render made
    in that session silently lacked Nanite and Lumen.

.PARAMETER Preflight
    Default mode. Reproduces UE's adapter enumeration and fails if the P40 is absent
    or is not capable of shader model 6.

.PARAMETER Verify
    Reads Saved\Logs\Aura.log and asserts that the session that actually ran got
    D3D12/SM6 on the P40. This is the authoritative check because it uses UE's own
    verdict rather than a reimplementation of it.

.PARAMETER LogFile
    With -Verify, check this log instead of Saved\Logs\Aura.log.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\PreflightRenderAdapter.ps1
    powershell -ExecutionPolicy Bypass -File Scripts\PreflightRenderAdapter.ps1 -Verify
#>
[CmdletBinding()]
param(
    [switch]$Preflight,
    [switch]$Verify,
    [string]$LogFile
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# DXGI / D3D12 probe
#
# The C# below walks the COM vtables directly instead of declaring the full
# ComImport interfaces, which keeps it short. Vtable slots were taken from the
# Windows SDK 10.0.22621.0 headers and verified by counting the members of each
# interface (see the slot comments).
# ---------------------------------------------------------------------------
$probeSource = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class AdapterProbe
{
    // IDXGIFactory6 = {c1b6694f-ff09-44a9-b03c-77900a0a1d17}
    static Guid IID_IDXGIFactory6 =
        new Guid("c1b6694f-ff09-44a9-b03c-77900a0a1d17");
    // IDXGIAdapter = {2411e7e1-12ac-4ccf-bd14-9798e8534dc0}
    static Guid IID_IDXGIAdapter =
        new Guid("2411e7e1-12ac-4ccf-bd14-9798e8534dc0");
    // ID3D12Device = {189819f1-1db6-4b57-be54-1821339b85f7}
    static Guid IID_ID3D12Device =
        new Guid("189819f1-1db6-4b57-be54-1821339b85f7");

    const int D3D_FEATURE_LEVEL_11_0 = 0xB000;
    const int DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE = 2;

    // D3D12_FEATURE enum values (d3d12.h)
    const int D3D12_FEATURE_SHADER_MODEL = 7;
    const int D3D12_FEATURE_D3D12_OPTIONS9 = 37;

    // IDXGIFactory6 vtable slot of EnumAdapterByGpuPreference. The full order,
    // counted from IDXGIFactory6Vtbl in dxgi1_6.h:
    //   0 QueryInterface            1 AddRef                   2 Release
    //   3 SetPrivateData            4 SetPrivateDataInterface  5 GetPrivateData  6 GetParent
    //   7 EnumAdapters              8 MakeWindowAssociation    9 GetWindowAssociation
    //  10 CreateSwapChain          11 CreateSoftwareAdapter
    //  12 EnumAdapters1            13 IsCurrent
    //  14 IsWindowedStereoEnabled  15 CreateSwapChainForHwnd  16 CreateSwapChainForCoreWindow
    //  17 GetSharedResourceAdapterLuid 18 RegisterStereoStatusWindow 19 RegisterStereoStatusEvent
    //  20 UnregisterStereoStatus   21 RegisterOcclusionStatusWindow 22 RegisterOcclusionStatusEvent
    //  23 UnregisterOcclusionStatus 24 CreateSwapChainForComposition
    //  25 GetCreationFlags         26 EnumAdapterByLuid        27 EnumWarpAdapter
    //  28 CheckFeatureSupport      29 EnumAdapterByGpuPreference
    const int SLOT_ENUM_ADAPTER_BY_GPU_PREFERENCE = 29;

    // IDXGIAdapter vtable slots
    const int SLOT_RELEASE = 2;
    const int SLOT_GET_DESC = 8;

    // ID3D12Device vtable slot of CheckFeatureSupport, counted from
    // ID3D12DeviceVtbl in d3d12.h:
    //   0 QueryInterface 1 AddRef 2 Release
    //   3 GetPrivateData 4 SetPrivateData 5 SetPrivateDataInterface 6 SetName
    //   7 GetNodeCount 8 CreateCommandQueue 9 CreateCommandAllocator
    //  10 CreateGraphicsPipelineState 11 CreateComputePipelineState
    //  12 CreateCommandList 13 CheckFeatureSupport
    const int SLOT_CHECK_FEATURE_SUPPORT = 13;

    [DllImport("dxgi.dll", ExactSpelling = true)]
    static extern int CreateDXGIFactory1(ref Guid riid, out IntPtr ppFactory);

    [DllImport("d3d12.dll", ExactSpelling = true)]
    static extern int D3D12CreateDevice(IntPtr pAdapter, int MinimumFeatureLevel,
                                        ref Guid riid, out IntPtr ppDevice);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int EnumAdapterByGpuPreferenceDel(IntPtr self, uint Adapter,
        int GpuPreference, ref Guid riid, out IntPtr ppvAdapter);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int GetDescDel(IntPtr self, IntPtr pDesc);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate uint ReleaseDel(IntPtr self);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int CheckFeatureSupportDel(IntPtr self, int Feature,
        IntPtr pFeatureSupportData, uint FeatureSupportDataSize);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct DXGI_ADAPTER_DESC
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public uint LuidLowPart;
        public int LuidHighPart;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct D3D12_OPTIONS9
    {
        public int MeshShaderPipelineStatsSupported;
        public int MeshShaderSupportsFullRangeRenderTargetArrayIndex;
        public int AtomicInt64OnTypedResourceSupported;
        public int AtomicInt64OnGroupSharedSupported;
        public int DerivativesInMeshAndAmplificationShadersSupported;
        public int WaveMMATier;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct D3D12_SHADER_MODEL_DATA
    {
        public int HighestShaderModel;
    }

    static T GetFn<T>(IntPtr pUnk, int slot)
    {
        IntPtr vtbl = Marshal.ReadIntPtr(pUnk);
        IntPtr fn = Marshal.ReadIntPtr(vtbl, slot * IntPtr.Size);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(fn, typeof(T));
    }

    static uint Release(IntPtr pUnk)
    {
        return GetFn<ReleaseDel>(pUnk, SLOT_RELEASE)(pUnk);
    }

    /// <summary>
    /// Returns one "|"-separated record per adapter, in the order Unreal Engine sees
    /// them: Index|Description|VendorId|DedicatedVRAMBytes|D3D12Create|ShaderModel|Atomic64
    /// </summary>
    public static string[] Enumerate()
    {
        List<string> rows = new List<string>();

        IntPtr factory;
        Guid factoryIid = IID_IDXGIFactory6;
        int hr = CreateDXGIFactory1(ref factoryIid, out factory);
        if (hr != 0)
        {
            throw new Exception("CreateDXGIFactory1(IDXGIFactory6) failed 0x"
                + hr.ToString("X8"));
        }

        try
        {
            EnumAdapterByGpuPreferenceDel enumFn =
                GetFn<EnumAdapterByGpuPreferenceDel>(factory,
                    SLOT_ENUM_ADAPTER_BY_GPU_PREFERENCE);

            for (uint i = 0; ; i++)
            {
                IntPtr adapter;
                Guid adapterIid = IID_IDXGIAdapter;
                int ehr = enumFn(factory, i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                 ref adapterIid, out adapter);
                if (ehr != 0 || adapter == IntPtr.Zero)
                {
                    break;
                }

                try
                {
                    DXGI_ADAPTER_DESC desc = new DXGI_ADAPTER_DESC();
                    int descSize = Marshal.SizeOf(typeof(DXGI_ADAPTER_DESC));
                    IntPtr descBuf = Marshal.AllocHGlobal(descSize);
                    string name = "<unknown>";
                    uint vendorId = 0;
                    ulong vram = 0;
                    try
                    {
                        GetFn<GetDescDel>(adapter, SLOT_GET_DESC)(adapter, descBuf);
                        desc = (DXGI_ADAPTER_DESC)Marshal.PtrToStructure(
                            descBuf, typeof(DXGI_ADAPTER_DESC));
                        name = desc.Description;
                        vendorId = desc.VendorId;
                        vram = (ulong)desc.DedicatedVideoMemory.ToUInt64();
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(descBuf);
                    }

                    string d3d12 = "FAILED";
                    int shaderModel = 0;
                    bool atomic64 = false;

                    IntPtr device;
                    Guid deviceIid = IID_ID3D12Device;
                    int chr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
                                                ref deviceIid, out device);
                    if (chr == 0 && device != IntPtr.Zero)
                    {
                        d3d12 = "OK";
                        try
                        {
                            D3D12_SHADER_MODEL_DATA sm = new D3D12_SHADER_MODEL_DATA();
                            sm.HighestShaderModel = 0x67; // query up to SM 6.7
                            int smSize = Marshal.SizeOf(typeof(D3D12_SHADER_MODEL_DATA));
                            IntPtr smBuf = Marshal.AllocHGlobal(smSize);
                            try
                            {
                                Marshal.StructureToPtr(sm, smBuf, false);
                                int shr = GetFn<CheckFeatureSupportDel>(device,
                                    SLOT_CHECK_FEATURE_SUPPORT)(device,
                                    D3D12_FEATURE_SHADER_MODEL, smBuf, (uint)smSize);
                                if (shr == 0)
                                {
                                    sm = (D3D12_SHADER_MODEL_DATA)Marshal.PtrToStructure(
                                        smBuf, typeof(D3D12_SHADER_MODEL_DATA));
                                    shaderModel = sm.HighestShaderModel;
                                }
                            }
                            finally
                            {
                                Marshal.FreeHGlobal(smBuf);
                            }

                            D3D12_OPTIONS9 o9 = new D3D12_OPTIONS9();
                            int o9Size = Marshal.SizeOf(typeof(D3D12_OPTIONS9));
                            IntPtr o9Buf = Marshal.AllocHGlobal(o9Size);
                            try
                            {
                                Marshal.StructureToPtr(o9, o9Buf, false);
                                int ohr = GetFn<CheckFeatureSupportDel>(device,
                                    SLOT_CHECK_FEATURE_SUPPORT)(device,
                                    D3D12_FEATURE_D3D12_OPTIONS9, o9Buf, (uint)o9Size);
                                if (ohr == 0)
                                {
                                    o9 = (D3D12_OPTIONS9)Marshal.PtrToStructure(
                                        o9Buf, typeof(D3D12_OPTIONS9));
                                    atomic64 = o9.AtomicInt64OnTypedResourceSupported != 0;
                                }
                            }
                            finally
                            {
                                Marshal.FreeHGlobal(o9Buf);
                            }
                        }
                        finally
                        {
                            Release(device);
                        }
                    }

                    rows.Add(string.Join("|", new string[] {
                        i.ToString(),
                        name,
                        "0x" + vendorId.ToString("X4"),
                        vram.ToString(),
                        d3d12,
                        shaderModel.ToString(),
                        atomic64 ? "true" : "false"
                    }));
                }
                finally
                {
                    Release(adapter);
                }
            }
        }
        finally
        {
            Release(factory);
        }

        return rows.ToArray();
    }
}
'@

function Format-ShaderModel {
    param([int]$Value)
    if ($Value -eq 0) { return 'n/a' }
    return ('{0}.{1}' -f (($Value -shr 4) -band 0xF), ($Value -band 0xF))
}

function Invoke-Preflight {
    if (-not ('AdapterProbe' -as [type])) {
        Add-Type -TypeDefinition $probeSource -Language CSharp | Out-Null
    }

    Write-Host '=== Unreal render adapter preflight ===' -ForegroundColor Cyan
    Write-Host ''
    Write-Host 'Enumerating with IDXGIFactory6::EnumAdapterByGpuPreference(HIGH_PERFORMANCE),'
    Write-Host 'which is exactly what UE 5.5 does when it picks its D3D12 adapter.'
    Write-Host ''

    $rows = [AdapterProbe]::Enumerate()
    if ($rows.Count -eq 0) {
        throw 'No DXGI adapters were returned.'
    }

    $adapters = @()
    foreach ($row in $rows) {
        $parts = $row -split '\|'
        $adapters += [PSCustomObject]@{
            Index      = [int]$parts[0]
            Name       = $parts[1]
            VendorId   = $parts[2]
            VramGB     = [math]::Round(([double]$parts[3]) / 1GB, 2)
            D3D12      = $parts[4]
            ShaderModel = Format-ShaderModel ([int]$parts[5])
            Atomic64   = ($parts[6] -eq 'true')
        }
    }

    $adapters | Format-Table -AutoSize |
        Out-String -Width 200 |
        ForEach-Object { Write-Host $_ }

    Write-Host 'Note: ShaderModel is queried through the system d3d12.dll, which on this OS tops'
    Write-Host '      out at 6.6. UE bundles a newer DirectX Agility SDK and reports 6.7 for the'
    Write-Host '      same adapters. The decisive gate is Atomic64, which matches UE exactly.'
    Write-Host ''

    $p40 = $adapters | Where-Object { $_.Name -like '*Tesla P40*' } | Select-Object -First 1

    Write-Host '--- Verdict ---' -ForegroundColor Cyan

    $failed = $false

    if (-not $p40) {
        Write-Host 'FAIL: the NVIDIA Tesla P40 is NOT present in the enumeration UE uses.' -ForegroundColor Red
        Write-Host '      UE will select the Intel iGPU, then fall back to D3D11/SM5, and' -ForegroundColor Red
        Write-Host '      Nanite and Lumen will be disabled for the whole session.' -ForegroundColor Red
        Write-Host '      Check Device Manager and that the NVIDIA driver finished loading.' -ForegroundColor Red
        $failed = $true
    }
    else {
        if ($p40.D3D12 -ne 'OK') {
            Write-Host "FAIL: P40 is enumerated (index $($p40.Index)) but D3D12CreateDevice failed." -ForegroundColor Red
            $failed = $true
        }
        if (-not $p40.Atomic64) {
            Write-Host "FAIL: P40 reports AtomicInt64OnTypedResourceSupported = false." -ForegroundColor Red
            Write-Host '      UE would compute SM5 and fall back to D3D11.' -ForegroundColor Red
            $failed = $true
        }
        if (-not $failed) {
            Write-Host ("OK: P40 found at UE adapter index {0}, {1} GB VRAM, D3D12 OK, shader model {2}, atomic64 supported." -f `
                $p40.Index, $p40.VramGB, $p40.ShaderModel) -ForegroundColor Green
            if ($p40.Index -ne 0) {
                Write-Host "NOTE: the P40 is not index 0. UE's default rule still picks it because it" -ForegroundColor Yellow
                Write-Host "      has the largest dedicated video memory of the non-integrated adapters." -ForegroundColor Yellow
            }
        }
    }

    # The iGPU is the thing that drags the engine down to SM5 when the P40 is missing.
    $igpu = $adapters | Where-Object { $_.Name -like '*Intel*' } | Select-Object -First 1
    if ($igpu -and -not $igpu.Atomic64) {
        Write-Host ("INFO: {0} reports shader model {1} but atomic64 = false." -f `
            $igpu.Name, $igpu.ShaderModel) -ForegroundColor DarkGray
        Write-Host '      UE requires AtomicInt64OnTypedResourceSupported for its D3D12/SM6 RHI path,' -ForegroundColor DarkGray
        Write-Host '      so for this adapter UE computes SM5 and then falls back to D3D11.' -ForegroundColor DarkGray
        Write-Host '      That is why losing the P40 costs the whole engine its D3D12/SM6 path.' -ForegroundColor DarkGray
    }

    Write-Host ''
    if ($failed) {
        Write-Host 'RESULT: FAILED - do not start the editor until this is resolved.' -ForegroundColor Red
        return 1
    }
    Write-Host 'RESULT: PASSED - a new editor session will render on the P40 with D3D12/SM6.' -ForegroundColor Green
    return 0
}

function Invoke-Verify {
    # Aura.log is the primary session log; UE rotates it to Aura-backup-*.log on each
    # launch, so it always describes the most recent session. Aura_2.log only exists
    # when a second editor instance ran concurrently, which is not what we want to
    # verify by default.
    if ($LogFile) {
        $log = Get-Item -Path $LogFile -ErrorAction Stop
    }
    else {
        $primary = Join-Path $ProjectRoot 'Saved\Logs\Aura.log'
        if (Test-Path $primary) {
            $log = Get-Item $primary
        }
        else {
            $log = Get-ChildItem -Path (Join-Path $ProjectRoot 'Saved\Logs') -Filter '*.log' -ErrorAction SilentlyContinue |
                   Where-Object { $_.Name -notlike 'cef3*' } |
                   Sort-Object LastWriteTime -Descending |
                   Select-Object -First 1
            if (-not $log) { throw 'No UE logs found under Saved\Logs.' }
        }
    }

    Write-Host "=== Verifying UE log: $($log.Name) ===" -ForegroundColor Cyan
    Write-Host "    Last written: $($log.LastWriteTime)"
    Write-Host ''

    $lines = Get-Content -Path $log.FullName

    $found = @{}
    $chosen = $null
    $sm6 = $false
    $d3d11Fallback = $false
    $naniteDisabled = 0

    foreach ($line in $lines) {
        if ($line -match 'Found D3D12 adapter (\d+): (.+?) \(VendorId') {
            $found[[int]$Matches[1]] = $Matches[2]
        }
        if ($line -match 'Chosen D3D12 Adapter Id = (\d+)') {
            $chosen = [int]$Matches[1]
        }
        if ($line -match 'RHI D3D12 with Feature Level SM6 is supported and will be used') {
            $sm6 = $true
        }
        if ($line -match 'attempting to fall back to RHI D3D11 with Feature Level SM5') {
            $d3d11Fallback = $true
        }
        if ($line -match 'UseNanite\(PCD3D_SM5\) returned false') {
            $naniteDisabled++
        }
    }

    Write-Host 'D3D12 adapters UE found:'
    if ($found.Count -eq 0) {
        Write-Host '  (none logged)' -ForegroundColor Yellow
    }
    foreach ($k in ($found.Keys | Sort-Object)) {
        Write-Host ("  [{0}] {1}" -f $k, $found[$k])
    }
    Write-Host ''

    $ok = $true
    if ($chosen -eq $null) {
        Write-Host 'FAIL: no "Chosen D3D12 Adapter Id" line - the log may be truncated.' -ForegroundColor Red
        $ok = $false
    }
    elseif (-not ($found.ContainsKey($chosen) -and $found[$chosen] -like '*Tesla P40*')) {
        Write-Host ("FAIL: UE chose adapter {0} ({1}), not the Tesla P40." -f $chosen, $found[$chosen]) -ForegroundColor Red
        $ok = $false
    }
    else {
        Write-Host ("OK: UE chose adapter {0} = {1}." -f $chosen, $found[$chosen]) -ForegroundColor Green
    }

    if ($sm6 -and -not $d3d11Fallback) {
        Write-Host 'OK: RHI D3D12 with Feature Level SM6 is in use.' -ForegroundColor Green
    }
    else {
        Write-Host 'FAIL: D3D12/SM6 was not used - the engine fell back to D3D11/SM5.' -ForegroundColor Red
        $ok = $false
    }

    if ($naniteDisabled -gt 0) {
        Write-Host ("FAIL: Nanite was disabled in this session ({0} occurrences of 'UseNanite(PCD3D_SM5) returned false')." -f $naniteDisabled) -ForegroundColor Red
        $ok = $false
    }
    else {
        Write-Host 'OK: no SM5/Nanite-disabled messages in this session.' -ForegroundColor Green
    }

    Write-Host ''
    if ($ok) {
        Write-Host 'RESULT: PASSED' -ForegroundColor Green
        return 0
    }
    Write-Host 'RESULT: FAILED - renders from this session are not trustworthy for Nanite/Lumen work.' -ForegroundColor Red
    return 1
}

if ($Verify) {
    exit (Invoke-Verify)
}
else {
    exit (Invoke-Preflight)
}
