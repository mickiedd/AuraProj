$ErrorActionPreference = 'SilentlyContinue'
$class = 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}'
Write-Host "=== GPU class subkeys ==="
Get-ChildItem $class | Where-Object { $_.PSChildName -match '^\d{4}$' } | ForEach-Object {
    $d = Get-ItemProperty $_.PSPath
    [PSCustomObject]@{
        Key          = $_.PSChildName
        DriverDesc   = $d.DriverDesc
        ProviderName = $d.ProviderName
        MatchingDeviceId = $d.MatchingDeviceId
    }
} | Format-Table -AutoSize

Write-Host "`n=== P40 InstanceId ==="
$p40 = Get-PnpDevice -PresentOnly -FriendlyName '*Tesla P40*'
$p40 | Select-Object FriendlyName,InstanceId,Status,Class | Format-List

Write-Host "`n=== Content of hardcoded 0002 key (what the script edits) ==="
$key2 = "$class\0002"
if (Test-Path $key2) {
    Get-ItemProperty $key2 | Select-Object DriverDesc,ProviderName,GridLicensedFeatures,AdapterType,EnableMsHybrid,FeatureScore,MatchingDeviceId | Format-List
} else {
    Write-Host "0002 key does NOT exist"
}

Write-Host "`n=== All display adapters (Get-CimInstance) ==="
Get-CimInstance Win32_VideoController | Select-Object Name,VideoProcessor,AdapterCompatibility,DriverVersion,CurrentHorizontalResolution,Status | Format-Table -AutoSize