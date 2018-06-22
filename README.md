# Zynq mkbootimage

(c) 2015-2018 Antmicro <www.antmicro.com>

This is an open-source replacement for the Xilinx bootgen application.
It parses a `.bif` file and creates a Zynq boot image in `.bin` format.

It is written entirely in C.

Required C libraries: `pcre`, `libelf`.

To build this application run:
```
make
```

To use it, type in:
```
./mkbootimage [--zynqmp|-u] <input_bif_file> <output_bin_file>
```

### Zynq-7000

Currently it supports creating boot image containing fsbl, bitstream,
u-boot, and linux related binary files.

For loading linux related images, both the `[load]` and the `[offset]` attributes
are supported.
As opposed to the original bootgen, the file extensions are not required.

For example the following .bif file:
```
the_ROM_image:
{
  [bootloader]fsbl.elf
  fpga.bit
  u-boot.elf
  [load=0x2a00000]devicetree.dtb
  [load=0x2000000]uramdisk
  [load=0x3000000]uImage
}
```

Used as follows:
```
./mkbootimage boot.bif boot.bin
```

will generate a bin image which can be used in u-boot, as follows:
```
bootm 0x3000000 0x2000000 0x2a00000
```

### Zynq Ultrascale+

Currently it supports creating boot image containing fsbl, bitstream,
u-boot, the arm trusted software and linux related binary images.

For example the following .bif file:
```
the_ROM_image:
{
  [fsbl_config] a53_x64
  [bootloader] fsbl.elf
  [destination_device=pl] fpga.bit
  [, destination_cpu=a53-0, exception_level=el-2] bl31.elf
  [, destination_cpu=a53-0, exception_level=el-2] u-boot.elf
  [load=0x2a00000]devicetree.dtb
  [load=0x2000000]uramdisk
  [load=0x3000000]uImage
}
```

Used as follows:
```
./mkbootimage --zynqmp boot.bif boot.bin
```

will generate a bin image, which can be used to successfully boot a Zynq
Ultrascale+ machine, and to boot Linux using U-Boot command:
```
bootm 0x3000000 0x2000000 0x2a00000
```

Encryption certificates are not supported.
