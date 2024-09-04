#First, install some necessary package
```
sudo apt install flex bison make vim gcc git -y
```

#clone the backport repo, compile and install it:
```
git clone https://github.com/ixianhao/intel-wifi.git
```
```
sudo cp intel-wifi/fw-binaries/*.ucode /lib/firmware
```
```
cd intel-wifi/iwlwifi-stack-dev
```
```
sudo make defconfig-iwlwifi-public
```
```
sudo make
```
```
sudo make install
```

#Third, download the driver, or you can download your driver from Intel website.
```
git clone git://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git
```
```
cd linux-firmware/
```
```
sudo cp iwlwifi-* /lib/firmware/
```

#Or download  drivers for url 

https://www.intel.com/content/www/us/en/support/articles/000005511/wireless.html

#Reboot the machine and it works for me
