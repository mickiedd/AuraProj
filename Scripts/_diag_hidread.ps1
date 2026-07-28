# _diag_hidread.ps1 — open the Switch Pro Controller (VID_057E PID_2009) over Bluetooth
# and read its HID input report directly, to see if usable stick data arrives WITHOUT
# the Nintendo activation handshake. Push the left/right sticks during the poll.
# If the report bytes change while you push, the data is available and the UE RawInput
# plugin can read it (just needs axis mapping). If the report is empty/static, the
# controller needs the activation handshake (BetterJoy/ds4windows) and RawInput alone won't work.

$src = @'
using System;
using System.Runtime.InteropServices;
using System.Threading;

public static class HID {
  [StructLayout(LayoutKind.Sequential)]
  public struct HIDD_ATTRIBUTES { public uint Size; public ushort VendorID; public ushort ProductID; public ushort VersionNumber; }

  [DllImport("hid.dll")] public static extern void HidD_GetHidGuid(out Guid HidGuid);
  [DllImport("hid.dll")] public static extern bool HidD_GetAttributes(IntPtr h, ref HIDD_ATTRIBUTES attr);
  [DllImport("hid.dll")] public static extern bool HidD_GetCaps(IntPtr h, IntPtr caps);
  [DllImport("hid.dll")] public static extern bool HidD_GetInputReport(IntPtr h, byte[] buf, uint len);
  [DllImport("hid.dll")] public static extern bool HidD_SetNumInputBuffers(IntPtr h, uint n);

  [DllImport("setupapi.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr SetupDiGetClassDevs(ref Guid g, IntPtr enumerator, IntPtr hwndParent, uint flags);
  [DllImport("setupapi.dll", CharSet=CharSet.Auto, SetLastError=true)]
  public static extern bool SetupDiEnumDeviceInterfaces(IntPtr hDevInfo, IntPtr devInfoData, ref Guid g, uint idx, ref SP_DEVICE_INTERFACE_DATA did);
  [DllImport("setupapi.dll", CharSet=CharSet.Auto, SetLastError=true)]
  public static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr hDevInfo, ref SP_DEVICE_INTERFACE_DATA did, IntPtr detail, uint len, ref uint req, IntPtr devInfoData);
  [DllImport("setupapi.dll")] public static extern bool SetupDiDestroyDeviceInfoList(IntPtr hDevInfo);
  [DllImport("kernel32.dll", CharSet=CharSet.Auto, SetLastError=true)]
  public static extern IntPtr CreateFile(string path, uint access, uint share, IntPtr sa, uint disp, uint flags, IntPtr template);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);

  [StructLayout(LayoutKind.Sequential)]
  public struct SP_DEVICE_INTERFACE_DATA { public uint cbSize; public Guid InterfaceClassGuid; public uint Flags; public IntPtr Reserved; }
}
'@
Add-Type -TypeDefinition $src -Language CSharp

$guid = [Guid]::Empty
[HID]::HidD_GetHidGuid([ref]$guid)
$hDev = [HID]::SetupDiGetClassDevs([ref]$guid, [IntPtr]::Zero, [IntPtr]::Zero, 0x12) # DIGCF_PRESENT|DIGCF_DEVICEINTERFACE
$idx = 0
$proHandle = [IntPtr]::Zero
while ($true) {
  $did = New-Object HID+SP_DEVICE_INTERFACE_DATA
  $did.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf($did)
  if (-not [HID]::SetupDiEnumDeviceInterfaces($hDev, [IntPtr]::Zero, [ref]$guid, $idx, [ref]$did)) { break }
  $idx++
  $req = 0
  [HID]::SetupDiGetDeviceInterfaceDetail($hDev, [ref]$did, [IntPtr]::Zero, 0, [ref]$req, [IntPtr]::Zero) | Out-Null
  $buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$req)
  [HID]::SetupDiGetDeviceInterfaceDetail($hDev, [ref]$did, $buf, $req, [ref]$req, [IntPtr]::Zero) | Out-Null
  # detail struct: uint cbSize; fixed char DevicePath[]
  $path = [System.Runtime.InteropServices.Marshal]::PtrToStringUni([IntPtr]($buf.ToInt64()+4))
  [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
  # open read-only
  $h = [HID]::CreateFile($path, [uint32]2147483648, [uint32]7, [IntPtr]::Zero, [uint32]3, [uint32]0, [IntPtr]::Zero) # GENERIC_READ, share R|W|DELETE, OPEN_EXISTING
  $le = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
  if ($h -eq [IntPtr]::new(-1)) {
    $attr0 = New-Object HID+HIDD_ATTRIBUTES
    $attr0.Size = [uint32][System.Runtime.InteropServices.Marshal]::SizeOf($attr0)
    # try to get attrs anyway is impossible without handle; just log path + err
    Write-Output ("  (skip, open failed err={0}) {1}" -f $le, $path)
    continue
  }
  $attr = New-Object HID+HIDD_ATTRIBUTES
  $attr.Size = [uint32][System.Runtime.InteropServices.Marshal]::SizeOf($attr)
  if ([HID]::HidD_GetAttributes($h, [ref]$attr)) {
    Write-Output ("  HID coll VID={0:X4} PID={1:X4} open=OK {2}" -f $attr.VendorID, $attr.ProductID, $path)
    if ($attr.VendorID -eq 0x057E -and $attr.ProductID -eq 0x2009) {
      Write-Output ("FOUND Pro Controller collection (open OK)")
      $proHandle = $h
      break
    }
  }
  [HID]::CloseHandle($h)
}
[HID]::SetupDiDestroyDeviceInfoList($hDev) | Out-Null

if ($proHandle -eq [IntPtr]::Zero -or $proHandle -eq [IntPtr](-1)) {
  Write-Output "Pro Controller (057E/2009) not openable. Make sure it is connected and not held by another app."
  return
}

[HID]::HidD_SetNumInputBuffers($proHandle, 2) | Out-Null
# Pro Controller BT input report 0x30 is up to 49 bytes (+ report ID). Use 64 to be safe.
$rlen = 64
Write-Output "==== polling HID input report for 8s (push sticks now) ===="
$capBuf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(20)
[HID]::HidD_GetCaps($proHandle, $capBuf) | Out-Null
[System.Runtime.InteropServices.Marshal]::FreeHGlobal($capBuf)

$last = ""
$changes = 0
$end = (Get-Date).AddSeconds(8)
while ((Get-Date) -lt $end) {
  $rb = New-Object byte[] $rlen
  $ok = [HID]::HidD_GetInputReport($proHandle, $rb, $rlen)
  if ($ok) {
    $hex = ($rb[0..13] | ForEach-Object { $_.ToString("X2") }) -join ' '
    if ($hex -ne $last) {
      Write-Output ("  report: " + $hex)
      $last = $hex
      $changes++
    }
  } else {
    Write-Output ("  HidD_GetInputReport FAILED (err=" + [System.Runtime.InteropServices.Marshal]::GetLastWin32Error() + ")")
    Start-Sleep -Milliseconds 500
  }
  Start-Sleep -Milliseconds 150
}
[HID]::CloseHandle($proHandle) | Out-Null
Write-Output ("==== done. distinct reports seen: {0}" -f $changes)
if ($changes -le 1) {
  Write-Output "VERDICT: No changing data -> controller is NOT sending stick reports. Activation handshake likely required (use BetterJoy/ds4windows)."
} else {
  Write-Output "VERDICT: Reports changing while pushing -> stick data IS available without handshake. UE RawInput plugin can read it (needs axis mapping)."
}