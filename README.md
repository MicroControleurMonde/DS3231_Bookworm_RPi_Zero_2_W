# DS3231 for Bookworm RPi Zero 2 W

DS3231 RTC Driver for Raspberry Pi Zero 2W (RPi Bookworm)


## DS3231 driver

This is a Linux driver for the DS3231 Real-Time Clock (RTC) module.

### Features:

- Supports reading and writing the time and date to the DS3231 RTC.
- Does not include alarm support.

### Tested Environment

The driver was developed for and tested with Raspberry Pi OS, but it should work with any Linux distribution that supports I2C and the rtc subsystem.

- Debian version: 12 (bookworm)
- Kernel version: 6.6.74
- Architecture: ARMv7 32-bit (rpt-rpi-v7, armv7l)
- GCC version: gcc-12
- Binutils: 2.40
- Raspbian version: 1:6.6.74-1+rpt1

**Note**: The source code once compiled should work on ALL Raspberry Pi boards using the same OS (Bookworm)

### Compiling the Driver

To compile the driver on your system, you can use the provided Makefile. Ensure that you have the necessary build tools and kernel headers installed.

1. **update** your system and install necessary packets

```bash
sudo apt-get update
sudo apt install git
```

3. **Clone this repository**:

```bash
https://github.com/MicroControleurMonde/DS3231_Bookworm_RPi_Zero_2_W.git
cd ds3231
```
3. **Install kernel headers** (if not already installed):

```bash
sudo apt-get install linux-headers-$(uname -r)
```
4. **Build** the driver:

```bash
make
```

5. **Load** the module:

```bash
sudo insmod ds3231.ko
```

6. **Check the driver is loaded and the RTC is accessible**:

```bash
dmesg | grep ds3231
```

7. **Use the rtc interface to set or read the time**:

```bash
sudo hwclock -r
sudo hwclock -w
```


### I2C Address

By default, the driver assumes the DS3231 RTC module is using the standard I2C address 0x68. The I2C address is implicitly managed through the use of I2C functions.

If you have changed the I2C address of your DS3231 module, you might need to modify the source code to reflect the new address.


### License

This driver is licensed under the GPL v3 license.




