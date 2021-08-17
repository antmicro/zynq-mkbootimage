# Zynq mkbootimage

Copyright (c) 2015-2021 [Antmicro](https://antmicro.com)

This is an open-source replacement for the Xilinx `bootgen` application.
The package provides sources of two binaries `mkbootimage` and `exbootimage`
for both creation and extraction of Zynq boot images.

The tools are written entirely in C.

Requires the `libelf` C library.

To build these the tools run:
```
make
```

## mkbootimage
`mkbootimage` parses a `.bif` file and creates a Zynq boot image in the `.bin` format.

To use it, type in:
```
./mkbootimage [--parse-only|-p] [--zynqmp|-u] <input_bif_file> <output_bin_file>
```

To see all available options, run:
```
./mkbootimage --help
```

### Zynq-7000

For Zynq-7000 series, `zynq-mkbootimage` currently supports creating boot images
containing the FSBL, bitstream, U-Boot, and Linux-related binary files.

For loading Linux-related images, both the `[load]` and the `[offset]` attributes
are supported.
As opposed to the original `bootgen` utility, file extensions are not required.

For example the following `.bif` file:
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

used with the following command:
```
./mkbootimage boot.bif boot.bin
```

will generate a `.bin` image which can be used in U-Boot, as follows:
```
bootm 0x3000000 0x2000000 0x2a00000
```

### Zynq UltraScale+

For Zynq UltraScale+, `zynq-mkbootimage` currently supports creating boot images
containing the FSBL, bitstream, U-Boot, ARM trusted software and Linux-related binary images.

For example the following `.bif` file:
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

used with the following command:
```
./mkbootimage --zynqmp boot.bif boot.bin
```

will generate a `.bin` image, which can be used to successfully boot a Zynq
UltraScale+ machine, and to boot Linux using the following U-Boot command:
```
bootm 0x3000000 0x2000000 0x2a00000
```

Encryption certificates are not supported.

## exbootimage
`exbootimage` parses a boot ROM file and extracts desired information out of it.

To use it, type in:
```
./exbootimage [--zynqmp|-u]   [--extract|-x] [--force|-f]  [--list|-l]
              [--describe|-d] [--header|-h]  [--images|-i] [--parts|-p]
              [--bitstream|-d DESIGN,PART-NAME]
              <input_bif_file> [extract_file...]
```

To see all available options, run:
```
./exbootimage --help
```

Three of the main functionalities of the tool are described below.

### Listing the contents of the boot image

To list the contents of the boot image use the `-l` option. It
is especially useful before extracting partitions.

To obtain the list, run:
```
./exbootimage -l boot.bin
```

### Printing a description of headers of the boot image
You can print a readable description of all headers in a boot image
by using the `-d` option.

The output is divided into sections dedicated to various header types:
1. Main file header
2. Image header table
3. Image headers
4. Partition headers

To see it working, run:
```
./exbootimage -d boot.bin
```

Or for ZynqMP bootimages:
```
./exbootimage -u -d boot.bin
```

### Extracting partition data
To extract partition contents use the `-x` option. The partitions
will be extracted into files named after each partition's image name.

To perform this operation, run:
```
./exbootimage -x boot.bin
```

Or for ZynqMP boot files:
```
./exbootimage -ux boot.bin
```

The tool stops if a file of that name is already present, this behaviour
can be bypassed with the `-f` flag:
```
./exbootimage -uxf boot.bin
```

To extract only some of the partitions, type their names after
the boot image name:
```
./exbootimage -x boot.bin fpga.bit rootfs.img
```

