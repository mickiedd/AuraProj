$src = @'
using System;
using System.Runtime.InteropServices;
public static class XInput {
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_CAPABILITIES {
    public byte Type; public byte SubType; public ushort Flags;
    public XINPUT_GAMEPAD Gamepad; public XINPUT_VIBRATION Vibration;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_GAMEPAD {
    public ushort wButtons; public byte bLeftTrigger; public byte bRightTrigger;
    public short sThumbLX; public short sThumbLY; public short sThumbRX; public short sThumbRY;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_VIBRATION { public ushort wLeftMotorSpeed; public ushort wRightMotorSpeed; }
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_STATE { public uint dwPacketNumber; public XINPUT_GAMEPAD Gamepad; }
  [DllImport("xinput1_4.dll")]
  public static extern uint XInputGetCapabilities(uint dwUserIndex, uint dwFlags, out XINPUT_CAPABILITIES pCapabilities);
  [DllImport("xinput1_4.dll")]
  public static extern uint XInputGetState(uint dwUserIndex, out XINPUT_STATE pState);
  [DllImport("xinput1_4.dll")]
  public static extern uint XInputEnable(uint enable);
}
'@
Add-Type -TypeDefinition $src -Language CSharp

[XInput]::XInputEnable(1) | Out-Null

Write-Output "==== XInput capabilities (users 0-3) ===="
for ($i=0; $i -lt 4; $i++) {
  $cap = New-Object XInput+XINPUT_CAPABILITIES
  $r = [XInput]::XInputGetCapabilities($i, 0, [ref]$cap)
  if ($r -eq 0) {
    Write-Output ("user {0}: CONNECTED  subtype={1}" -f $i,$cap.SubType)
  } else {
    Write-Output ("user {0}: not connected (err=0x{1:X})" -f $i,$r)
  }
}

Write-Output "==== live state poll user 0 (~10s, push+HOLD the left stick now) ===="
$maxL = 0
for ($s=0; $s -lt 34; $s++) {
  Start-Sleep -Milliseconds 300
  $st = New-Object XInput+XINPUT_STATE
  $r = [XInput]::XInputGetState(0, [ref]$st)
  if ($r -eq 0) {
    $mL = [Math]::Max([Math]::Abs($st.Gamepad.sThumbLX), [Math]::Abs($st.Gamepad.sThumbLY))
    if ($mL -gt $maxL) { $maxL = $mL }
    if ($mL -gt 1800) {
      Write-Output ("  t={0:N1}s btn=0x{1:X4} L=({2},{3}) R=({4},{5}) LT={6} RT={7}" -f ($s*0.3),$st.Gamepad.wButtons,$st.Gamepad.sThumbLX,$st.Gamepad.sThumbLY,$st.Gamepad.sThumbRX,$st.Gamepad.sThumbRY,$st.Gamepad.bLeftTrigger,$st.Gamepad.bRightTrigger)
    }
  } else {
    Write-Output ("  t={0:N1}s not connected (err=0x{1:X})" -f ($s*0.3),$r)
  }
}
Write-Output ("==== max |left-stick| seen = {0} (XInput deadzone is {1}) ====" -f $maxL, 7849)
if ($maxL -gt 1800) { Write-Output "VERDICT: BetterJoy IS outputting stick values to XInput -> issue is UE-side (EnhancedInput)." }
elseif ($maxL -gt 0) { Write-Output "VERDICT: tiny stick value only -> BetterJoy range/scale problem (calibration/range setting)." }
else { Write-Output "VERDICT: zero stick -> BetterJoy is NOT outputting sticks. Check BetterJoy stick/calibration settings." }