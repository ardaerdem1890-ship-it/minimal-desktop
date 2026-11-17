# Capture a screenshot from the running VM via VirtualBox's built-in screenshot
$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.4.1"
$out = "D:\Varsayılan Proje\minimal-desktop\build\shots\vm141.png"
& $vb controlvm $vm screenshotpng $out 2>&1
if (Test-Path $out) { Get-Item $out | Select-Object FullName,Length } else { Write-Output "no screenshot" }
