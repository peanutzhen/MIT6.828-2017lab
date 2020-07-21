

## Ubuntu16配置MIT6.828-2017环境

### **0.为什么使用ubuntu16.04来作为环境呢？**

首先，这是官方推荐的，推荐我们在linux虚拟机配置环境。这样就算配置环境时搞坏了系统，也不怕。所以最好不要在自己的电脑环境直接配置，在虚拟机配置，等你做完了实验，删除环境也很简单。

选择16.04是因为作者的虚拟机软件parallel desktop对更新版本的ubuntu安装parallel tools会失败，且解决方法比较麻烦。

### **1.安装git**

`sudo apt install git`

### **2.下载学校打了补丁的qemu-6.828-v2.3.0**

`git clone http://web.mit.edu/ccutler/www/qemu.git -b 6.828-2.3.0`

### **3.安装必要库**

> On Linux, you may need to install several libraries. We have successfully built 6.828 QEMU on Debian/Ubuntu 16.04 after installing the following packages: libsdl1.2-dev, libtool-bin, libglib2.0-dev, libz-dev, and libpixman-1-dev.

这段话就是说，你需要在安装前对ubuntu16安装一些库。这些库都可以使用apt安装。

```shell
sudo apt install libsdl1.2-dev
sudo apt install libtool-bin
sudo apt install libglib2.0-dev 
sudo apt install libz-dev 
sudo apt install libpixman-1-dev
```



### **4.配置qemu的安装信息**

`./configure --disable-kvm [--prefix=PFX] [--target-list="i386-softmmu x86_64-softmmu"]`

中扩号里的是可选option，建议默认prefix就行，建议保留target-list这个参数（可以减少qemu编译所需的依赖），不然会出现ERROR: DTC (libfdt) not present.的错误

### **5.make编译安装**

`sudo make && sudo make install`

记得加sudo，否则安装时在/usr/local/share创建文件时会提示不够权限。

### **6.安装gcc-multilib库**

为了Lab1的内核能够顺利make，我们必须还需安装这个库。

`sudo apt install gcc-multilib `

这样，我们的环境就算是配置好了！可以开始愉快的玩耍了！