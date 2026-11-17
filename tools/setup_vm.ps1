$ErrorActionPreference = "Stop"
$vb = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$vm = "minimal-1.4.1"
$iso = "D:\Varsayılan Proje\minimal-desktop\build\minimal_desktop.iso"

& $vb modifyvm $vm --memory 256 --vram 16 --boot1 dvd --boot2 disk --boot3 none --boot4 none
& $vb modifyvm $vm --nic1 nat --nictype1 82540EM
& $vb modifyvm $vm --audio none
& $vb storagectl $vm --name "IDE" --add ide --controller PIIX4 --bootable on
& $vb createmedium disk --filename "D:\Varsayılan Proje\minimal-desktop\build\test_disk.vdi" --size 512 --format VDI
& $vb storageattach $vm --storagectl "IDE" --port 0 --device 0 --type hdd --medium "D:\Varsayılan Proje\minimal-desktop\build\test_disk.vdi"
& $vb storageattach $vm --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium $iso
Write-Output "VM configured:"
& $vb showvminfo $vm --machinereadable | Select-String -Pattern '^(VMState|memory|boot1|IDE-0-0|IDE-1-0|nic1|nictype1)='
