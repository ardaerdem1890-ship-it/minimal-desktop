$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.7.0"
$map = @{
  'a'=0x1E;'b'=0x30;'c'=0x2E;'d'=0x20;'e'=0x12;'f'=0x21;'g'=0x22;'h'=0x23
  'i'=0x17;'j'=0x24;'k'=0x25;'l'=0x26;'m'=0x32;'n'=0x31;'o'=0x18;'p'=0x19
  'q'=0x10;'r'=0x13;'s'=0x1F;'t'=0x14;'u'=0x16;'v'=0x2F;'w'=0x11;'x'=0x2D
  'y'=0x15;'z'=0x2C;'1'=0x02;'2'=0x03;'3'=0x04;'4'=0x05;'5'=0x06;'6'=0x07
  '7'=0x08;'8'=0x09;'9'=0x0A;'0'=0x0B;' '=0x39;'-'=0x0C;'.'=0x34;'/'=0x35
}
function Send([string]$t) {
  foreach ($ch in $t.ToCharArray()) {
    $c = $map[[string]$ch]; if ($null -eq $c) { continue }
    & $vb controlvm $vm keyboardputscancode ('{0:X2}' -f $c) ('{0:X2}' -f ($c -bor 0x80)) 2>$null
    Start-Sleep -Milliseconds 60
  }
  & $vb controlvm $vm keyboardputscancode 1C 9C 2>$null
  Start-Sleep -Milliseconds 300
}
# reboot from disk
& $vb controlvm $vm poweroff 2>$null | Out-Null; Start-Sleep 3
& $vb storageattach $vm --storagectl IDE --port 1 --device 0 --type dvddrive --medium none 2>$null
& $vb modifyvm $vm --boot1 disk 2>$null
& $vb startvm $vm --type gui 2>$null | Out-Null; Start-Sleep 12
& $vb controlvm $vm screenshotpng "D:\prelogin.png" 2>$null
# login as arda / 123321
Send "arda"; Start-Sleep 1; Send "123321"; Start-Sleep 2
& $vb controlvm $vm screenshotpng "D:\ardalogin.png" 2>$null
# whoami
Send "whoami"; Start-Sleep 1
& $vb controlvm $vm screenshotpng "D:\ardawho.png" 2>$null
Write-Output "done"
