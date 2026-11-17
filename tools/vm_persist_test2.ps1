$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.4.1"
$iso = "D:\md141.iso"
$map = @{
  'a'=0x1E;'b'=0x30;'c'=0x2E;'d'=0x20;'e'=0x12;'f'=0x21;'g'=0x22;'h'=0x23
  'i'=0x17;'j'=0x24;'k'=0x25;'l'=0x26;'m'=0x32;'n'=0x31;'o'=0x18;'p'=0x19
  'q'=0x10;'r'=0x13;'s'=0x1F;'t'=0x14;'u'=0x16;'v'=0x2F;'w'=0x11;'x'=0x2D
  'y'=0x15;'z'=0x2C;'1'=0x02;'2'=0x03;'3'=0x04;'4'=0x05;'5'=0x06;'6'=0x07
  '7'=0x08;'8'=0x09;'9'=0x0A;'0'=0x0B;' '=0x39;'-'=0x0C;'.'=0x34;'/'=0x35
  '='=0x0D;','=0x33
}
function Send([string]$t) {
  foreach ($ch in $t.ToCharArray()) {
    $c = $map[[string]$ch]; if ($null -eq $c) { continue }
    & $vb controlvm $vm keyboardputscancode ('{0:X2}' -f $c) ('{0:X2}' -f ($c -bor 0x80)) 2>$null
    Start-Sleep -Milliseconds 40
  }
  & $vb controlvm $vm keyboardputscancode 1C 9C 2>$null
}
function Reboot([string]$mode) {
  & $vb controlvm $vm poweroff 2>$null | Out-Null
  Start-Sleep 3
  & $vb modifyvm $vm --boot1 $mode --boot2 dvd 2>$null
  & $vb startvm $vm --type gui 2>$null | Out-Null
  Start-Sleep 12
}

Reboot "dvd"
Send "install"; Start-Sleep 2; Send "y"
Write-Output "waiting for install..."
Start-Sleep 90
& $vb controlvm $vm screenshotpng "D:\v3_install.png" 2>$null

Reboot "disk"
Send "write /disk/persist.txt SURVIVED"; Start-Sleep 3
Send "sync"; Start-Sleep 3
& $vb controlvm $vm screenshotpng "D:\v3_write.png" 2>$null

Reboot "disk"
Send "cat /disk/persist.txt"; Start-Sleep 3
& $vb controlvm $vm screenshotpng "D:\v3_persist.png" 2>$null
Write-Output "done"
