$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.8.0"
$iso = "D:\md180.iso"
$vdi = "D:\md180_disk.vdi"

& $vb controlvm $vm poweroff 2>$null | Out-Null
Start-Sleep 1
& $vb unregistervm $vm --delete 2>$null | Out-Null
if (Test-Path $vdi) { Remove-Item $vdi -Force }

& $vb createvm --name $vm --register --ostype Other
& $vb modifyvm $vm --memory 256 --vram 16 --boot1 dvd --boot2 disk --boot3 none --boot4 none
& $vb modifyvm $vm --nic1 nat --nictype1 Am79C973
& $vb modifyvm $vm --audio none
& $vb storagectl $vm --name "IDE" --add ide --controller PIIX4 --bootable on
& $vb createmedium disk --filename $vdi --size 512 --format VDI
& $vb storageattach $vm --storagectl "IDE" --port 0 --device 0 --type hdd --medium $vdi
& $vb storageattach $vm --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium $iso
& $vb startvm $vm --type gui
Write-Output "--- state ---"
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^(VMState|boot1|IDE-0-0|IDE-1-0)='
Write-Output "--- disk file ---"
Get-Item $vdi | Select-Object FullName,Length
