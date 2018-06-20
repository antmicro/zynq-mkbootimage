#ifndef BOOTROM_H
#define BOOTROM_H

#include <gelf.h>
#include <bif.h>

#define BOOTROM_SUCCESS 0
#define BOOTROM_ERROR_NOFILE 1
#define BOOTROM_ERROR_BITSTREAM 2
#define BOOTROM_ERROR_ELF 3
#define BOOTROM_ERROR_SEC_OVERLAP 4
#define BOOTROM_ERROR_UNSUPPORTED 5
#define BOOTROM_ERROR_NOMEM 6

/* BootROM Header based on ug585 and ug1085 */
typedef struct bootrom_hdr_t {
  uint32_t interrupt_table[8];
  uint32_t width_detect;
  uint32_t img_id;
  uint32_t encryption_status;
  union {
    uint32_t user_defined_0;
    uint32_t fsbl_defined_0;
    uint32_t fsbl_execution_addr;
  };
  uint32_t src_offset;
  union {
    struct {
      uint32_t img_len;
      uint32_t reserved_0; /* set to 0 */
    };
    struct {
      uint32_t pmufw_len;
      uint32_t pmufw_total_len;
    };
  };
  union {
    uint32_t start_of_exec;
    uint32_t fsbl_img_len;
  };
  uint32_t total_img_len;
  union {
    uint32_t reserved_1;
    uint32_t fsbl_target_cpu;
  };
  uint32_t checksum;
  /* The rest of the header is different for zynq and zynqmp*/
  union {
    /* This is the zynq part */
    struct {
      union {
        uint32_t user_defined_zynq_0[21];
        uint32_t fsbl_defined_zynq_0[21];
      };
      uint32_t reg_init_zynq[512];
      union {
        uint32_t user_defined_zynq_1[8];
        uint32_t fsbl_defined_zynq_1[8];
      };
    };
    /* This is the zynqmp part */
    struct {
      uint32_t obfuscated_key[8];
      uint32_t reserved_zynqmp;
      union {
        uint32_t user_defined_zynqmp_0[12];
        uint32_t fsbl_defined_zynqmp_0[12];
      };
      uint32_t sec_hdr_init_vec[3];
      uint32_t obf_key_init_vec[3];
      uint32_t reg_init_zynqmp[512];
      /* 0xFF padding ?? */
      uint32_t padding[2];
    };
  };
} bootrom_hdr_t;

/* BootROM image header table based on ug821 and ug1137 */
typedef struct bootrom_img_hdr_tab_t {
  uint32_t version;
  uint32_t hdrs_count;
  uint32_t part_hdr_off; /* word offset to the partition header */
  uint32_t part_img_hdr_off; /* word offset to first image header */
  uint32_t auth_hdr_off; /* word offset to header authentication */
  union {
    /* just padding for zynq */
    uint32_t padding[11];

    /* zynqmp additions */
    struct {
      uint32_t boot_dev;
      uint32_t reserved[9];
      uint32_t checksum;
    };
  };
} bootrom_img_hdr_tab_t;

/* BootROM partition header based on ug821 */
/* All offsets are relative to the start of the boot image */
typedef struct bootrom_partition_hdr_t {
  uint32_t pd_len;
  uint32_t ed_len;
  uint32_t total_len;

  uint32_t arch_specific[12];

  uint32_t checksum;
} bootrom_partition_hdr_t;

/* Output image specific parameters */
#define BOOTROM_IMG_MAX_NAME_LEN 32
#define BOOTROM_IMG_PADDING_SIZE 64

/* BootROM image header based on ug821 */
typedef struct bootrom_img_hdr_t {
  uint32_t next_img_off; /* 0 if last */
  uint32_t part_hdr_off;
  uint32_t part_count; /* always set to 0 */
  /* Name length is not really the length of the name.
   * According to the documentation it is the value of the
   * actual partition count, however the bootgen binary
   * always sets this field to 1. */
  uint32_t name_len;
  uint8_t  name[BOOTROM_IMG_MAX_NAME_LEN];
  uint8_t  padding[16];
} bootrom_img_hdr_t;

typedef struct linux_image_header_t {
  uint32_t magic;
  uint32_t hcrc;
  uint32_t time;
  uint32_t size;
  uint32_t load;
  uint32_t ep;
  uint32_t dcrc;
  uint8_t os;
  uint8_t arch;
  uint8_t type;
  uint8_t comp;
  uint8_t name[32];
} linux_image_header_t;

/* attributes of the bootrom partition header */
#define BOOTROM_PART_ATTR_OWNER_OFF      16
#define BOOTROM_PART_ATTR_OWNER_MASK     (3 << BOOTROM_PART_ATTR_OWNER_OFF)
#define BOOTROM_PART_ATTR_OWNER_FSBL     (0 << BOOTROM_PART_ATTR_OWNER_OFF)
#define BOOTROM_PART_ATTR_OWNER_UBOOT    (1 << BOOTROM_PART_ATTR_OWNER_OFF)

#define BOOTROM_PART_ATTR_RSA_USED_OFF   15
#define BOOTROM_PART_ATTR_RSA_USED_MASK  (1 << BOOTROM_PATR_ATTR_RSA_USED_OFF)
#define BOOTROM_PART_ATTR_RSA_USED       (1 << BOOTROM_PART_ATTR_RSA_USED_OFF)
#define BOOTROM_PART_ATTR_RSA_NOT_USED   (0 << BOOTROM_PART_ATTR_RSA_USED_OFF)

#define BOOTROM_PART_ATTR_DEST_CPU_OFF   8
#define BOOTROM_PART_ATTR_DEST_CPU_NONE  (0 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_A53_0 (1 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_A53_1 (2 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_A53_2 (3 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_A53_3 (4 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_R5_0  (5 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_R5_1  (6 << BOOTROM_PART_ATTR_DEST_CPU_OFF)
#define BOOTROM_PART_ATTR_DEST_CPU_R5_L  (7 << BOOTROM_PART_ATTR_DEST_CPU_OFF)

#define BOOTROM_PART_ATTR_ENCRYPTION_OFF 7
#define BOOTROM_PART_ATTR_ENCRYPTION_YES (1 << BOOTROM_PART_ATTR_ENCRYPTION_OFF)
#define BOOTROM_PART_ATTR_ENCRYPTION_NO  (0 << BOOTROM_PART_ATTR_ENCRYPTION_OFF)

#define BOOTROM_PART_ATTR_DEST_DEV_OFF   4
#define BOOTROM_PART_ATTR_DEST_DEV_MASK  (7 << BOOTROM_PART_ATTR_DEST_DEV_OFF)
#define BOOTROM_PART_ATTR_DEST_DEV_NONE  (0 << BOOTROM_PART_ATTR_DEST_DEV_OFF)
#define BOOTROM_PART_ATTR_DEST_DEV_PS    (1 << BOOTROM_PART_ATTR_DEST_DEV_OFF)
#define BOOTROM_PART_ATTR_DEST_DEV_PL    (2 << BOOTROM_PART_ATTR_DEST_DEV_OFF)
#define BOOTROM_PART_ATTR_DEST_DEV_INT   (3 << BOOTROM_PART_ATTR_DEST_DEV_OFF)

#define BOOTROM_PART_ATTR_A5X_EXEC_S_OFF 3
#define BOOTROM_PART_ATTR_A5X_EXEC_S_64  (0 << BOOTROM_PART_ATTR_A5X_EXEC_S_OFF)
#define BOOTROM_PART_ATTR_A5X_EXEC_S_32  (1 << BOOTROM_PART_ATTR_A5X_EXEC_S_OFF)

#define BOOTROM_PART_ATTR_EXC_LVL_OFF    1
#define BOOTROM_PART_ATTR_EXC_LVL_MASK   (0x3 << BOOTROM_PART_ATTR_EXC_LVL_OFF)
#define BOOTROM_PART_ATTR_EXC_LVL_EL0    (0 << BOOTROM_PART_ATTR_EXC_LVL_OFF)
#define BOOTROM_PART_ATTR_EXC_LVL_EL1    (1 << BOOTROM_PART_ATTR_EXC_LVL_OFF)
#define BOOTROM_PART_ATTR_EXC_LVL_EL2    (2 << BOOTROM_PART_ATTR_EXC_LVL_OFF)
#define BOOTROM_PART_ATTR_EXC_LVL_EL3    (3 << BOOTROM_PART_ATTR_EXC_LVL_OFF)

#define BOOTROM_PART_ATTR_TRUST_ZONE_OFF 0
#define BOOTROM_PART_ATTR_TRUST_ZONE_YES (1 << BOOTROM_PART_ATTR_TRUST_ZONE_OFF)
#define BOOTROM_PART_ATTR_TRUST_ZONE_NO  (0 << BOOTROM_PART_ATTR_TRUST_ZONE_OFF)

/* values taken from boot.bin generated with bootgen
 * so there is no understanding of them yet */
#define BOOTROM_INT_TABLE_DEFAULT 0xEAFFFFFE
#define BOOTROM_RESERVED_1_RL     0x00000001 /* MUST be set to 0 but is not */
#define BOOTROM_RESERVED_ZMP_RL   0x01000020 /* The same as img version? */

/* user defined 0 / fsbl execution address */
#define BOOTROM_USER_0            0x01010000 /* used by zynq */
#define BOOTROM_FSBL_EXEC_ADDR    0xfffc0000 /* used by zynqmp */

/* PMU firmware related */
/* The maximum PMU FW size should be 0x00020000 (128kB)
 * according to the TRM, but for some reason bootgen uses
 * the value used below */
#define BOOTROM_PMUFW_MAX_SIZE    0x0001fae0

/* these values are also taken from bootm.bin
 * however these might not be the only valid
 * option - that is, in theory they could be
 * dynamic */
#define BOOTROM_IMG_HDR_OFF       0x000008c0
#define BOOTROM_PART_HDR_OFF      0x00000c80
#define BOOTROM_PART_HDR_END_PADD 0x0000003c
#define BOOTROM_BINS_OFF          0x00001700

/* The same defines as above but for zynqmp */
#define BOOTROM_BINS_OFF_ZMP      0x00000b40

/* values from the documentation */
#define BOOTROM_WIDTH_DETECT      0xAA995566
#define BOOTROM_IMG_ID            "XNLX"
#define BOOTROM_ENCRYPTED_EFUSE   0xA5C3C5A3
#define BOOTROM_ENCRYPTED_OEFUSE  0xA5C3C5A7 /* obfuscated key in eFUSE */
#define BOOTROM_ENCRYPTED_RAMKEY  0x3A5C3C5A /* bram */
#define BOOTROM_ENCRYPTED_OBHDR   0xA35C7CA5 /* obfuscated key in boot hdr */
#define BOOTROM_ENCRYPTED_NONE    0x00000000 /* anything but efuse, ramkey*/
#define BOOTROM_MIN_SRC_OFFSET    0x000008C0
#define BOOTROM_RESERVED_0        0x00000000 /* MUST be set to 0 */
#define BOOTROM_RESERVED_1        0x00000000 /* MUST be set to 0 */

#define BOOTROM_IMG_VERSION       0x01020000

#define BOOTROM_IMG_HDR_BOOT_SAME 0x0
#define BOOTROM_IMG_HDR_BOOT_QSPI 0x1
#define BOOTROM_IMG_HDR_BOOT_NAND 0x2
#define BOOTROM_IMG_HDR_BOOT_SD   0x3
#define BOOTROM_IMG_HDR_BOOT_MMC  0x4
#define BOOTROM_IMG_HDR_BOOT_USB  0x5
#define BOOTROM_IMG_HDR_BOOT_ETH  0x6
#define BOOTROM_IMG_HDR_BOOT_PCIE 0x7
#define BOOTROM_IMG_HDR_BOOT_SATA 0x8

/* TODO find definitions for other CPUs */
#define BOOTROM_FSBL_CPU_R5       0x001
#define BOOTROM_FSBL_CPU_A53_64   0x800

/* Other file specific files */
#define FILE_MAGIC_ELF            0x464C457F

#define FILE_MAGIC_XILINXBIT_0    0xf00f0900
#define FILE_MAGIC_XILINXBIT_1    0xf00ff00f

#define FILE_MAGIC_LINUX          0x56190527
#define FILE_MAGIC_DTB            0xedfe0dd0

#define FILE_XILINXBIT_SEC_START  13
#define FILE_XILINXBIT_SEC_DATA   'e'

#define FILE_LINUX_IMG_TYPE_UIM   2
#define FILE_LINUX_IMG_TYPE_URD   3
#define FILE_LINUX_IMG_TYPE_SCR   6

#define BINARY_ATTR_LINUX         0x00
#define BINARY_ATTR_RAMDISK       0x02
#define BINARY_ATTR_GENERAL       0x01

typedef struct bootrom_offs_t {
  /* pointer to the image in memory */
  uint32_t *img_ptr;

  /* offsets used as pointers */
  uint32_t *coff; /* current offset/ptr */
  uint32_t *poff; /* current partiton offset */
  uint32_t *hoff; /* partition header table offset */

  /* offsets used as regular values */
  uint32_t img_hdr_off;
  uint32_t part_hdr_off;
  uint32_t part_hdr_end_off;
  uint32_t bins_off;
} bootrom_offs_t;

/* bootrom operations */
typedef struct bootrom_ops_t {
  /* Initialize offsets - image pointer should be
   * set before this one is called */
  int (*init_offs)(uint32_t*, int, bootrom_offs_t*);

  /* Initialize the main bootrom header */
  int (*init_header)(bootrom_hdr_t*, bootrom_offs_t*);

  /* Setup bootloader at the current offset */
  int (*setup_fsbl_at_curr_off)(bootrom_hdr_t*,
                                bootrom_offs_t*,
                                uint32_t img_len);

  /* Prepare image header table */
  int (*init_img_hdr_tab)(bootrom_img_hdr_tab_t*,
                          bootrom_img_hdr_t*,
                          bootrom_partition_hdr_t*,
                          bootrom_offs_t*);

  /* Partition header related callbacks */
  int (*init_part_hdr_default)(bootrom_partition_hdr_t*,
                               bif_node_t*);
  int (*init_part_hdr_dtb)(bootrom_partition_hdr_t*,
                           bif_node_t*);
  int (*init_part_hdr_elf)(bootrom_partition_hdr_t*,
                           bif_node_t*,
                           uint32_t *size,
                           uint32_t load,
                           uint32_t entry,
                           uint8_t nbits);
  int (*init_part_hdr_bitstream)(bootrom_partition_hdr_t*,
                                 bif_node_t*);
  int (*init_part_hdr_linux)(bootrom_partition_hdr_t*,
                             bif_node_t*,
                             linux_image_header_t*);
  /* The finish function is common for all partition types */
  int (*finish_part_hdr)(bootrom_partition_hdr_t*,
                         uint32_t *img_size,
                         bootrom_offs_t*);

  /* Some archs require a null partition at the end */
  uint8_t append_null_part;
} bootrom_ops_t;

uint32_t estimate_boot_image_size(bif_cfg_t*);
int create_boot_image(uint32_t*, bif_cfg_t*, bootrom_ops_t*, uint32_t*);

#endif /* BOOTROM_H */
