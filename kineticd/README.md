# Kineticd Lamarr C++ Implementation#

**Protocol Version:** Kineticd is using version `4.0.1` of the [Kinetic-Protocol](https://github.com/Kinetic/kinetic-protocol)

This is an overview on how to build, run, and manipulate Kineticd
on Ubuntu based Operating Systems. Please see uboot-linux repository for installation related documents

---

## Requirements For Getting Started: ##

**Install The Following Tools** `sudo apt-get install <tool-name>`

*   Git
*   cmake
*   minicom
*   Valgrind
*   GNU Make
*   DHCP server `isc-dhcp-server`
*   recent C++ compiler with support for C++11 _(gcc-4.8 g++-4.8)_
*   **Note:** `install_kv_dev.sh` in the [Kinetic Tools / Developer-Environment](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/developer-environment/browse) repository will download the tools automatically

**Acquire the following Items:**

*   Standard Lamarr Host Aware Disk Drive _(see partitioning section below)_
*   Serial Port Cable _(for minicom)_


**Clone The Following** Internal Repositories (other than this one):

*   [Uboot-Linux](http://lco-esd-cm01.colo.seagate.com:7990/projects/KIN/repos/uboot-linux/browse)
*   [Kinetic Python Client](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/python-client/browse)
*   [Kinetic Tools / Source](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/source/browse)
*   [Kinetic Tools / Shenzi](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/shenzi/browse)
*   [Kinetic Tools / Developer Environment](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/developer-environment/browse)


            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kin/kineticd.git
            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kt/python-client.git
            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kt/developer-environment.git
            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kt/shenzi.git
            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kt/source.git
            git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kin/uboot-linux.git

**Important:** Read Sections on **X86 Testing** and **Requirements for Contributing**

- - -

# Building

## Building for ARM - Cross Compilation

1.  Building uboot-linux before kineticd

    *  First build uboot-linux before the kineticd build, as kineticd build requires packages built by Buildroot
        
                $> cd uboot-linux
                $> ./build_embedded_image.sh -t ramdef
        [This step needs to be done only once (unless some changes are pushed). All kineticd builds can point to this location for further builds.]

    *  Build kineticd

                $> cd kineticd

                You will need to source the `envsetup.sh` file within the `uboot-linux` repository
                $> source ../<path-to-uboot-linux>/envsetup.sh
                
                Once you have sourced `envsetup.sh`, you will **only be able to build for ARM**. If you need to build for a separate architecture, use a new shell/terminal instance (or `unset` the environment variables)
                
                $> cmake -DPRODUCT=LAMARRKV
                
                $> make clean all

2.  Using kineticd's hash

        $> cd uboot-linux
        In file buildroot-2012.11-2015_T1.0/configs/lamarrkv_ramfs_buildroot_defconfig, replace the value of BR2_PACKAGE_KINETIC_KINETICD_GIT_REF with the desired kineticd's hash.
        $> ./build_embedded_image.sh -t ramdef

## Building for X86

You will need to source the `x86_envsetup.sh` file within the `kineticd` repository

        $> source x86_envsetup.sh

[Note: If the above script complains that few packages are not installed or if they are not located in the same location, run kineticd/x86_package_installation.sh]

Once you have sourced `x86_envsetup.sh`, you will **only be able to build for X86**. If you need to build for a separate architecture, use a new shell/terminal instance (or `unset` the environment variables)

#### Compiling Kineticd Source ####

        $> cmake -DPRODUCT=X86
        $> make clean all

#### Compile Time Debug Flag ####
`kineticd/src/debug.h`

To enable output for all `DLOG(INFO)` statements,
Comment out `#define NDEBUG` and re-compile

<!-- break -->
#### Cleaning The Build ####

Unfortunately, the CMake-generated `make clean` target doesn't clean everything. Git Clean should take care of it all:

        $> git clean -xfd
        $> rm -rf vendor

#### Customizing the Make File ####

To build a customized `Makefile` for your environment. You only need to re-run
CMake when you update `CMakeLists.txt`.  Finally, run `make` to build the
server and tests

#### Adding New Source Files ####
The addition of a new `.cc` file requires an update to `CMakeLists.txt` and a
re-run of CMake

#### Coding Style & Expectations ####
We follow _most_ of the style guidelines from the [Google C++ style manual](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml)
An easy way to check that your working copy adheres to the style guide is to run `lint.sh`


      # From top of Kineticd Directory
      $> ./lint.sh

- - -

# Installing to Kinetic Device #

Automated scripts, other build types, and a GUI based installation tool exists in the uboot-linux repository. Please see the [uboot-linux repo](http://lco-esd-cm01.colo.seagate.com:7990/projects/KIN/repos/uboot-linux/browse) for more details

**Manually Build Default uImage Installer**

1.   Enter Uboot Lamarr Directory

        cd uboot-linux/

2.   You will need to source the `envsetup.sh` file

        $> source ../<path-to-uboot-linux>/envsetup.sh

3.   **Boot-loader Tools**: Build the u-boot boot-loader and associated tools

        cd u-boot-2013.01-2015_T1.0
        ./build.pl -c -b armada_38x_customer0_1g -f spi -v 2013.01-2015_T1.0_1g

    Resulting File: `u-boot-a38x-2013.01-2015_T1.0_1g-spi.bin`

4.   **Build-Root**: Build the Root File System for our Linux Kernel

        cd ../buildroot-2012.11-2015_T1.0/
        make clean
        make lamarrkv_ramfs_buildroot_defconfig
        make

5.   Copy resulting Root File System archive into Linux directory

        cd ../linux-3.10.70-2015_T1.1/
        cp ../buildroot-2012.11-2015_T1.0/output/images/rootfs.cpio.lzma lamarrkv-ram-rootfs-buildroot.cpio.lzma

6.   **Default Kernel Image:** Build the default Linux zImage defined by `lamarrkv_ramfs_defconfig`

        make mrproper
        make lamarrkv_ramfs_defconfig
        make KALLSYMS_EXTRA_PASS=1 -j8 zImage

     Resulting File: `arch/arm/boot/zImage`

7.   **Device Tree Blob**: build the default DTB and concatenate it to the default zImage

        make lamarrkv.dtb
        cat arch/arm/boot/zImage arch/arm/boot/dts/lamarrkv.dtb > zImage.dtb

8.   **uImage**: Generate a u-boot friendly uImage with u-boot provided script

        /bin/bash ./scripts/mkuboot.sh -A arm -O linux -T kernel -C none -a 0x00008000 -e 0x00008000 -n "Name of Your Image" -d zImage.dtb arch/arm/boot/uImage

    Resulting File: `arch/arm/boot/uImage`

9.   **Installer Executable**: Generate a self-extracting ".run" file containing the uImage and required tools

        cd ../installer/
        cp cp ../linux-3.10.70-2015_T1.1/arch/arm/boot/uImage ./uImage
        ./generate-ram-installer.sh uImage

    Resulting File: `kineticd-ram-installer-<date>.run`

10.   **Sign the Firmware**: if you intend to install via the kinetic API interface, you must sign the firmware using the installer GUI. (see uboot repo for more information)

    **Note:** You must have signing privileges on the TDCI server

11.   **Deliver the Signed .slod to Drive**: using the Python Client, issue a `update_firmware()` command with your firmware
    Refer [Python/kinetic_py_client/examples/UpdateFirmware.py](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/python-client/browse/examples/UpdateFirmware.py)


12.   **Alternative Installation**: If you cannot sign the firmware and the drive port is unlocked. You can `scp` the `.run` file created in step **9**

    Example:

        $> scp kineticd-ram-installer.run root@192.168.0.13:/mnt/util/tmp/

    Connect to the Device via Serial Port (minicom) or via SSH. Go to the directory `/mnt/util/tmp` and execute the `.run` file

        you@ubuntu-workstation:~$ ssh root@192.168.0.13

        Port is Already UN-Locked

        [root@Z84090PT ~]# killkv
        [root@Z84090PT ~]# cd /mnt/util/tmp/
        [root@Z84090PT tmp]# ls
        kineticd-ram-installer.run
        [root@Z84090PT tmp]# ./kineticd-ram-installer.run

**Building kineticd installer using scripts**

1.  Using kineticd's hash

        $> python build_target.py -i [GID] -g [kinetic_hash] individual
        
        This builds both uboot-linux, kineticd and also signs the code and generates an installer

2.  Using pre-compiled location of kineticd [ with this method, you have to sign the code yourself after generating the executable ]

    *  Perform steps mentioned under **"Building uboot-linux before kineticd"**

    *  Build the uboot-linux again with the precompiled location of kineticd

                $> cd uboot-linux
                $> ./build_embedded_image.sh -t ramdef -c [PRECOMPILED_LOCATION_OF_KINETICD]

**NOTE:** See the uboot-linux repo for more information on installation, other installation types _e.g._ _Factory uImage_, updating u-boot, installer GUI tools, and how to manipulate Linux configurations

- - -

# Requirements for Contributing and Creating Pull Requests #

* Associate the names of new development branches with JIRA tickets

* **Before** a feature branch is ready to be merged, the following tasks must be carried out;
    * Follow the style guide and verify changes with `lint.sh`
    * Merge the Master branch into the development branch to verify no conflicts exist
    * Provide Examples of how to test changes
    * Run the local X86 Tests and make sure they Pass
    * Check for leaks with Valgrind
    * Write a summary of your changes on the pull request overview
    * Provide a URL to the Pull Request in the associated JIRA ticket description

- - -

# Running the Local X86 Server & X86 Tests #

You **must have** a **standard Host Aware Lamarr** Disk Drive available to your Host machine. It must have one partition

Follow the list of steps below to run the X86 Server:

1. Compile Kineticd Source for X86
2. Generate an SSL Certificate and Key _(see Generating and SSL section below)_
3. Compile & Insert **MemMgr** Module _(see MemMgr section below)_
4. Run kineticd with `sudo`, specify `store_partition` and `metadata_db_path`

* **Run Standard X86 Kineticd**

        $> sudo ./kineticd --store_partition=/dev/<yourdevice> --metadata_db_path=/metadata.db


* **Run X86 Test Suite (All Tests)**

        $> sudo ./kinetic_test --store_test_partition=/dev/<yourdeviceid> --metadata_db_path=/metadata.db


* **Run Subset of Unit Tests X86 Example**

    For details on supported filters see the [gtest advanced guide](https://code.google.com/p/googletest/wiki/V1_6_AdvancedGuide#Running_a_Subset_of_the_Tests)

        sudo ./kinetic_test --gtest_filter="SkinnyWaistTest/*GetPrev*"--store_test_partition=/dev/<yourdevice> --metadata_db_path=/metadata.db


- - -

#### Generating an SSL Certificate and Key ####

To create a SSL certificate and private key for use with the X86 build run the following command:

```
$> openssl req -x509 -nodes -newkey rsa:2048 -keyout private_key.pem -out certificate.pem
```

OpenSSL will ask you some simple questions; all of which can be left blank _(though you are free to fill them out if you wish)_

A key file called `private_key.pem` and certificate file called `certificate.pem` will be created

- - -

#### Loading MemMgr Module ####
[MemMgr Module Repo](http://lco-esd-cm01.colo.seagate.com:7990/projects/KT/repos/source/browse)

1. Build `.ko` files for module
2. Build for preferred Architecture (x86 Linux or ARM)
3. Insert module in kernel
4. sudo mknod /dev/memMgr c `<device-value>` 0

    _Example Module Insertion & Node Creation:_

        $> cd memMgr
        $> cp Makefile.linux Makefile  #copy Makefile.arm for ARM
        $> make
        $> sudo insmod memMgr_drv.ko
        $> cat /proc/devices | grep mem
          1 mem
          248 memMgrdrv
        $> sudo mknod /dev/memMgr c 248 0

- - -

#### Partitioning Lamarr Device ####
Partition your local Legacy Lamarr Host Aware device

* Run with Administrative privileges

        sudo gdisk /dev/<device>

* Create 1 Partition with default values (example output)

        Command (? for help): n
        Partition number (1-128, default 1): 1
        First sector (34-15628053134, default = 2048) or {+-}size{KMGTP}:
        Last sector (2048-15628053134, default = 15628053134) or {+-}size{KMGTP}:
        Current type is 'Linux filesystem'
        Hex code or GUID (L to show codes, Enter = 8300):
        Changed type of partition to 'Linux filesystem'

* Write Partition to Device (example output)

        Command (? for help): w

        Final checks complete. About to write GPT data. THIS WILL OVERWRITE EXISTING
        PARTITIONS!!

        Do you want to proceed? (Y/N): y
        OK; writing new GUID partition table (GPT) to /dev/sdb.
        The operation has completed successfully.


- - -

### Memory Leaks with Valgrind ###
For valgrind, **follow X86 compilation** steps and run the following command:

      sudo valgrind --leak-check=full --show-reachable=yes suppressions=valgrind_linux.supp ./kineticd --store_partition=/dev/sdb --metadata_db_path=/metadata.db



And to run under helgrind to check for threading issues try this:

        valgrind --tool=helgrind --suppressions=valgrind_linux.supp  ./kineticd


- - -

# Miscellaneous Items #

#### Killing Kineticd Process on Device ####
Stopping the Kineticd & kinetic_runner executables on the embedded Linux kernel

* Linux kernel associated with kinetic version `v05.00.13` onwards

    Simply type the command `killkv` on the device command line:

        [root@Z84090PT ~] killkv
        Removing Kinetic Runner respawn and Killing Kineticd

* For Linux kernel versions **earlier** than `v05.00.13`, carry out the following steps on the command line to stop kineticd on a device;

    1. Verify that kinetic_runner is active: `ps ax | grep kinetic`
    2.  Edit file `/etc/inittab` and comment out the line `null::respawn:/opt/kinetic/kinetic_runner`
    2. Save the `/etc/inittab` file and exit the editor
    2. run `kill -HUP 1`
    3. run `pkill kineticd`
    4. to verify kineticd & kinetic_runner are no longer running: `ps -ax | grep kinetic` and verify you do not see any instance of `/opt/kinetic/kinetic_runner` or `/opt/kinetic/kineticd`


---

#### Linux Serial Port Setup ####

1.   **Required Items**: Serial to USB cable, and `minicom` application
2.   Run minicom, substitute `<portnum>` with the USB port you will using on your machine

        sudo minicom --color=on /dev/ttyUSB1

4. Once running, hit `CTRL-A` then `Z` keys to bring up the menu
5. Hit the letter `o` key to access config submenu
6. Set the available options to match the following

        Serial Device: /dev/ttyUSB0
        Lockfile Location: /var/lock
        Callin Program:
        Callout Program:
        Bps/Par/Bits : 115200 8N1
        Hardware Flow Control : No
        Software Flow Control : Yes


---

**Running Host Aware Command Set (ARM and X86)**

It is possible run the Host Aware Command set without doing a full Kineticd build. To do so, change to the `ha_zac_cmds` directory. The file `main.cc` provides an area to write / execute the different commands. The resulting binary will be named `zac_local`.  An x86 and ARM unit test suite is also available (covered after zac_local overview)


* **Building Zac_local For ARM**
    1. `source` uboot-linux/envsetup.sh
    2. `arm_crosscompile`
    3. `make ARM=yes`
    4. Produces executable `zac_local`
    5. To Run; scp executable to Host Aware LamarrKV
        * `./zac_local /dev/sda`


* **Build Zac_local for X86**
    * `make`
    * Produces executable `zac_local`
    * To Run; need a host aware drive on your x86 workstation, pass the drive handle to the executable
        * `sudo ./zac_local /dev/sdb`


* **zac_ha_exercise_drive executable**
    * On _full_ kineticd builds, a binary named `zac_ha_exercise_drive.cc` will be generated for both x86 and ARM builds that can be run independently of kinetic and kinetic tests


* **zac_kinetic_mediator_test**
    * Unit Test Suite for X86 will run at the end of all other X86 tests
    * To Run as Standalone Test:
        * `git clean -xfd`
        * `cmake -DPRODUCT=X86 . && make clean all`
        * `sudo ./kinetic_test --gtest_filter="ZacKineticMediatorTest*"`

---

###### TODO Section ######

* Update Associated U-boot Documentation (for updating uboot, creating other kernels etc.)
* Clean Development Environment Setup and add a script in respective repos for installing the required packages. [JIRA](https://jira.seagate.com/jira/browse/ASOALBANY-120)

**Last Updated:** Mon 21 October 2019