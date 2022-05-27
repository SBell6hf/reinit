## reinit
Restart init.

```text
Usage:
 reinit [-i <newinit>] [-r <newroot> [-o <putold>]] [options] [-- init_options]

Restart init.

Options:
 -i, --newinit  the file to execute as the new init;     default: current init
 -r, --newroot  the mountpoint to use as new root;       default: /
 -o, --putold   the path to mount the old root if --newroot is set;  default: /._tmp_reinit/putold
 -b, --bbinit   use busybox as the new init; you'll need to spicify an applet in init_options

 -k, --nokill      do not kill old processes; PIDs of old processes will be stored in environment variable $oldproc
 -s, --nostop      do not stop old processes (implies --nokill); may make the kernel panic when used with -r
 -f, --keepfd      do not close fds for the new init
 -u, --umount      umount /proc, /dev, /sys, /run, /tmp (default when newroot == / && !nostop)
     --no-umount   do not umount /proc, /dev, /sys, /run, /tmp (default)

 -h, --help     display this help
 -V, --version  display version
```
Examples:
```bash
### DO NOT COPY & PASTE (PSEUDO CODE) ###
### DO NOT COPY & PASTE ###
### DO NOT COPY & PASTE ###

1. Reinstall OS without a USB stick:

# First stop as many services and sockets as possible.
pacman -S arch-install-scripts
mkdir /live
pacstrap -c /live base iwd dhcpcd vim busybox e2fsprogs btrfsprogs dosfstools openssh arch-install-scripts
cp -a /usr/lib/modules /live/usr/lib/
chroot /live passwd -d root
chroot /live passwd   # set a password for root
# You might need to configure sshd and network interfaces if you want to ssh into the machine.
chroot /live systemctl enable iwd dhcpcd
./reinit -r /live -o /mnt
# Wait about 5 sec.
# Login as root, and you are now in a live recovery environment. You can umount /mnt, format the disk, and install another linux.
# After the installation, you can use this utility again to enter your new dist (if you don't mind running an old version of linux kernel).

2. Convert an ext4 root to btrfs

pacman -S arch-install-scripts
mkdir /live
pacstrap -c /live base iwd dhcpcd vim busybox e2fsprogs btrfsprogs dosfstools openssh arch-install-scripts
cp -a /usr/lib/modules /live/usr/lib/
chroot /live passwd
cp * /mnt/root/
./reinit -r /live -o /mnt
# Wait.
umount -R /mnt
btrfs-convert /dev/DISK_DEVICE
mount /dev/DISK_DEVICE /mnt
mount /dev/ESP_DEVICE /mnt/boot/efi
genfstab -U /mnt >/mnt/etc/fstab
arch-chroot /mnt grub-install --removable
arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg
arch-chroot /mnt mkinitpio -P
./reinit -r /mnt -o /live
# Wait.
umount -R /live
```
