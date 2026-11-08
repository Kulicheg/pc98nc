/*
 * Automatically generated C config: don't edit
 */
#define AUTOCONF_INCLUDED

/*
 * Kernel & Hardware
 */

/*
 * Just accept the defaults unless you know what you are doing
 */

/*
 * System
 */
#define CONFIG_ARCH_IBMPC 1
#undef  CONFIG_ARCH_8018X
#undef  CONFIG_ARCH_PC98

/*
 * Platform
 */
#undef  CONFIG_HW_COMPAQFAST
#undef  CONFIG_HW_MK88

/*
 * Devices
 */
#define CONFIG_HW_VGA 1
#define CONFIG_HW_SERIAL_FIFO 1

/*
 * BIOS support
 */
#define CONFIG_APM 1

/*
 * Kernel settings
 */
#define CONFIG_BOOTOPTS 1
#define CONFIG_CPU_USAGE 1
#undef  CONFIG_TIME_RTC_LOCALTIME
#undef  CONFIG_TIME_TZ
#undef  CONFIG_STRACE
#undef  CONFIG_TIMER_INT0F
#undef  CONFIG_TIMER_INT1C
#undef  CONFIG_ROMCODE
#define CONFIG_FARTEXT_KERNEL 1

/*
 * Networking Support
 */
#define CONFIG_SOCKET 1
#undef  CONFIG_NANO
#define CONFIG_INET 1
#undef  CONFIG_UNIX

/*
 * Filesystem Support
 */
#define CONFIG_MINIX_FS 1
#undef  CONFIG_ROMFS_FS
#define CONFIG_FS_FAT 1
#define CONFIG_FS_DEV 1

/*
 * Filesystem settings
 */
#undef  CONFIG_ROOT_READONLY
#undef  CONFIG_FS_RO
#define CONFIG_FULL_VFS 1
#define CONFIG_32BIT_INODES 1
#define CONFIG_FS_EXTERNAL_BUFFER 1
#define CONFIG_FS_NR_EXT_BUFFERS (64)
#undef  CONFIG_FS_XMS_BUFFER
#define CONFIG_PIPE 1

/*
 * Executable file formats
 */
#define CONFIG_EXEC_COMPRESS 1
#define CONFIG_EXEC_MMODEL 1
#define CONFIG_EXEC_MMODEL 1

/*
 * Drivers
 */

/*
 * Block device drivers
 */
#define CONFIG_BLK_DEV_BIOS 1
#define CONFIG_BLK_DEV_BFD 1
#undef  CONFIG_BLK_DEV_BFD_HARD
#define CONFIG_TRACK_CACHE 1
#define CONFIG_BLK_DEV_BHD 1
#define CONFIG_IDE_PROBE 1
#undef  CONFIG_DMA

/*
 * Additional block devices
 */
#define CONFIG_BLK_DEV_RAM 1
#define CONFIG_RAMDISK_SEGMENT 0x0
#define CONFIG_RAMDISK_SECTORS (128)
#define CONFIG_BLK_DEV_SSD_NONE 1
#undef  CONFIG_BLK_DEV_SSD_TEST
#undef  CONFIG_BLK_DEV_SSD_SD8018X

/*
 * Block device options
 */
#define CONFIG_BLK_DEV_CHAR 1

/*
 * Character device drivers
 */
#define CONFIG_CONSOLE_DIRECT 1
#undef  CONFIG_CONSOLE_BIOS
#undef  CONFIG_CONSOLE_8018X
#undef  CONFIG_CONSOLE_HEADLESS
#define CONFIG_KEYBOARD_SCANCODE 1
#undef  CONFIG_CONSOLE_SERIAL
#define CONFIG_EMUL_ANSI 1
#undef  CONFIG_EMUL_VT52
#undef  CONFIG_EMUL_NONE
#undef  CONFIG_EMUL_ANSI_PRINTK
#undef  CONFIG_KEYMAP_BE
#undef  CONFIG_KEYMAP_DE
#undef  CONFIG_KEYMAP_DV
#undef  CONFIG_KEYMAP_ES
#undef  CONFIG_KEYMAP_FR
#undef  CONFIG_KEYMAP_IT
#undef  CONFIG_KEYMAP_SE
#undef  CONFIG_KEYMAP_UK
#define CONFIG_KEYMAP_US 1

/*
 * Other character devices
 */
#define CONFIG_CHAR_DEV_RS 1
#undef  CONFIG_CHAR_DEV_LP
#undef  CONFIG_CHAR_DEV_CGATEXT
#define CONFIG_CHAR_DEV_MEM 1
#define CONFIG_CHAR_DEV_MEM_PORT_READ 1
#define CONFIG_CHAR_DEV_MEM_PORT_WRITE 1
#define CONFIG_PSEUDO_TTY 1
#undef  CONFIG_DEV_META

/*
 * Network device drivers
 */
#define CONFIG_ETH 1
#define CONFIG_ETH_NE2K 1
#define CONFIG_ETH_WD 1
#define CONFIG_ETH_EL3 1

/*
 * Userland
 */
#undef  CONFIG_APPS_BY_IMAGESZ

/*
 * Shell
 */
#define CONFIG_APP_ASH 1
#define CONFIG_APP_SASH 1

/*
 * Applications
 */
#define CONFIG_APP_DISK_UTILS 1
#define CONFIG_APP_FILE_UTILS 1
#define CONFIG_APP_SH_UTILS 1
#define CONFIG_APP_SYS_UTILS 1
#define CONFIG_APP_ELVIS 1
#define CONFIG_APP_MINIX1 1
#define CONFIG_APP_MINIX2 1
#define CONFIG_APP_MINIX3 1
#define CONFIG_APP_MISC_UTILS 1
#define CONFIG_APP_TUI 1
#define CONFIG_APP_NANOX 1
#define CONFIG_APP_BASIC 1
#define CONFIG_APP_SCREEN 1
#define CONFIG_APP_CRON 1
#undef  CONFIG_APP_OTHER
#undef  CONFIG_APP_TEST
#undef  CONFIG_APP_BUSYELKS
#undef  CONFIG_APP_CGATEXT
#undef  CONFIG_APP_TAGS

/*
 * Internet stack and utilities
 */
#define CONFIG_APP_KTCP 1

/*
 * Man pages
 */
#undef  CONFIG_APP_MAN_PAGES

/*
 * System startup
 */
#undef  CONFIG_SYS_DEFSHELL_SASH
#undef  CONFIG_SYS_NO_BININIT

/*
 * Target image
 */
#define CONFIG_IMG_MINIX 1
#undef  CONFIG_IMG_FAT
#undef  CONFIG_IMG_RAW
#undef  CONFIG_IMG_ROM
#undef  CONFIG_IMG_FD2880
#define CONFIG_IMG_FD1440 1
#undef  CONFIG_IMG_FD1232
#undef  CONFIG_IMG_FD1200
#undef  CONFIG_IMG_FD720
#undef  CONFIG_IMG_FD360
#undef  CONFIG_IMG_HD
#define CONFIG_IMG_DEV 1
#define CONFIG_IMG_BOOT 1
#undef  CONFIG_APPS_COMPRESS

/*
 * Binary Images
 */
#undef  CONFIG_IMG_EXTRA_IMAGES
