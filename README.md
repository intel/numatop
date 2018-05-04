
## Building & Installing NumaTOP

Numatop uses autotools. If you're compiling from git, run `autogen.sh`
and then `make`. Otherwise, use `./configure && make`.

To install, run `sudo make install`.

To run the test program, run `make check` after compilation or check
the `mgen` program for help information.


## Build Dependencies

NumaTOP requires following libraries:

* numactl-devel or libnuma-dev(el)
* libncurses
* libpthread


## Supported Kernels

The recommended kernel version is the latest stable kernel, currently 4.15.

The minimum kernel version supported is 3.16

For Haswell supporting, please also apply a perf patch on 3.16. The patch
is `kernel_patches/0001-perf-x86-Widen-Haswell-OFFCORE-mask.patch`.

The patch can also be found at following link:
http://www.gossamer-threads.com/lists/linux/kernel/1964864


## Directories

common:	common code for all platforms.

intel : Intel platform-specific code.

powerpc: PowerPC platform-specific code.

test  : mgen source code. mgen is a micro-test application which can
        generate memory access with runtime latency value among CPUs.
        Note that this application is only used for numatop testing!

kernel_patches: the required kernel patches.


## Supported Hardware

numatop is supported on Intel Xeon processors: 5500-series, 6500/7500-series,
5600 series, E7-x8xx-series, and E5-16xx/24xx/26xx/46xx-series. 

E5-16xx/24xx/26xx/46xx-series had better be updated to latest CPU microcode
(microcode must be 0x618+ or 0x70c+).

To learn about NumaTOP, please visit http://01.org/numatop


## PowerPC Support

NumaTOP is also supported on PowerPC. Please check powerpc/FEATURES file
for more details.
