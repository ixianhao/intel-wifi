#第一步，让我们安装一些依赖

sudo apt install flex bison make vim gcc git -y

#第二步，下载驱动文件并编译

git clone https://gitee.com/ixianhao/intel-wifi.git

sudo cp intel-wifi/fw-binaries/*.ucode /lib/firmware

cd intel-wifi/iwlwifi-stack-dev

sudo make defconfig-iwlwifi-public

sudo make

sudo make install

#第三步，下载官网驱动文件


git clone git://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git

cd linux-firmware/

sudo cp iwlwifi-* /lib/firmware/

#或者在官网下载固件Linux Support for Intel® Wireless Adapters ，将解压后的ucode文件复制在/lib/firmware目录。

https://www.intel.com/content/www/us/en/support/articles/000005511/wireless.html

#第四步，重启你的电脑！
