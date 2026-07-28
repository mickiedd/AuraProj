# Fast targeted enumeration: HID game controllers + Steam/virtual driver presence.
Write-Output "==== HID game controllers (InstanceId encodes VID/PID for wired; BT shows BT GUID) ===="
Get-PnpDevice -Class HIDClass -ErrorAction SilentlyContinue |
  Where-Object { $_.FriendlyName -match "game controller|gamepad" } |
  ForEach-Object {
    Write-Output ("Status={0}  Name={1}" -f $_.Status, $_.FriendlyName)
    Write-Output ("  InstanceId={0}" -f $_.InstanceId)
    try {
      $parent = $_ | Get-PnpDeviceProperty -KeyName DEVPKEY_Device_Parent -ErrorAction SilentlyContinue
      if ($parent) { Write-Output ("  Parent={0}" -f $parent.Data) }
    } catch {}
  }

Write-Output "==== Nintendo VID_057E devices (any class) ===="
Get-PnpDevice -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -match "VID_057E" } |
  Select-Object Status, FriendlyName, InstanceId | Format-List

Write-Output "==== Steam / virtual gamepad driver processes ===="
$names = "steam","steamwebhelper","vJoy","ds4windows","BetterJoy","HidHide","ViGEmBus","x360ce","SCPControl"
Get-Process -ErrorAction SilentlyContinue | Where-Object { $names -contains $_.Name } | Select-Object Name,Id | Format-Table -AutoSize
Write-Output "(if nothing above, none of those are running)"