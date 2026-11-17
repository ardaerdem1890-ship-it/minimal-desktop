$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.4.1"
$iso = "D:\md141.iso"
$vdi = "D:\md141_disk.vdi"
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^IDE'
Write-Output "--- attach cd ---"
& $vb storageattach $vm --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium $iso
Write-Output "--- result ---"
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^(IDE-0-0|IDE-1-0)='
