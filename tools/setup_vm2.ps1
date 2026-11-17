$ErrorActionPreference = "Stop"
$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.4.1"
$iso = "D:\md141.iso"
$vdi = "D:\md141_disk.vdi"

# remove any partial attachment
& $vb storageattach $vm --storagectl "IDE" --port 0 --device 0 --type hdd --medium none 2>$null
& $vb storageattach $vm --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium none 2>$null
if (Test-Path $vdi) { Remove-Item $vdi -Force }

& $vb createmedium disk --filename $vdi --size 512 --format VDI
& $vb storageattach $vm --storagectl "IDE" --port 0 --device 0 --type hdd --medium $vdi
& $vb storageattach $vm --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium $iso
Write-Output "VM attachments:"
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^(IDE-0-0|IDE-1-0|boot1|boot2)='
