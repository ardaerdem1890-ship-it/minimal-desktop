$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.6.0"
$iso = "D:\md160.iso"
$vdi = "D:\md160_disk.vdi"
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
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^(VMState|boot1|IDE-1-0)='
