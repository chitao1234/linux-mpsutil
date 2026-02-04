/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LINUX_MPSUTIL_MPT_IOCTL_H
#define LINUX_MPSUTIL_MPT_IOCTL_H

/*
 * Minimal userspace copy of the mpt3sas management ioctl ABI.
 *
 * Linux keeps these definitions in a driver-private header
 * (drivers/scsi/mpt3sas/mpt3sas_ctl.h). For a standalone utility we carry
 * just what we need for read-only "show" style commands.
 *
 * Note: this header intentionally uses fixed-width integer types and plain
 * userspace pointers (void *).
 */

#include <stdint.h>
#include <sys/ioctl.h>

#define MPT2SAS_DEV_NAME "mpt2ctl"
#define MPT3SAS_DEV_NAME "mpt3ctl"

#define MPT3_MAGIC_NUMBER 'L'

/* Default ioctl timeout used by the kernel driver if timeout==0. */
#define MPT3_IOCTL_DEFAULT_TIMEOUT 10

struct mpt3_ioctl_header {
	uint32_t ioc_number;
	uint32_t port_number;
	uint32_t max_data_size;
};

struct mpt3_ioctl_pci_info {
	union {
		struct {
			uint32_t device : 5;
			uint32_t function : 3;
			uint32_t bus : 24;
		} bits;
		uint32_t word;
	} u;
	uint32_t segment_id;
};

#define MPT2_IOCTL_VERSION_LENGTH 32

struct mpt3_ioctl_iocinfo {
	struct mpt3_ioctl_header hdr;
	uint32_t adapter_type;
	uint32_t port_number;
	uint32_t pci_id;
	uint32_t hw_rev;
	uint32_t subsystem_device;
	uint32_t subsystem_vendor;
	uint32_t rsvd0;
	uint32_t firmware_version;
	uint32_t bios_version;
	uint8_t driver_version[MPT2_IOCTL_VERSION_LENGTH];
	uint8_t rsvd1;
	uint8_t scsi_id;
	uint16_t rsvd2;
	struct mpt3_ioctl_pci_info pci_information;
};

/*
 * Generic "MPI request passthrough" ioctl.
 *
 * The request bytes are provided starting at mf[0]. The kernel will copy
 * data_sge_offset*4 bytes into its internal request frame; then it will build
 * SGEs starting at that offset based on data_in_size/data_out_size.
 *
 * This is why mf is declared as [1] in the kernel ABI: callers are expected
 * to allocate extra trailing bytes beyond sizeof(struct mpt3_ioctl_command).
 */
struct mpt3_ioctl_command {
	struct mpt3_ioctl_header hdr;
	uint32_t timeout;
	void *reply_frame_buf_ptr;
	void *data_in_buf_ptr;
	void *data_out_buf_ptr;
	void *sense_data_ptr;
	uint32_t max_reply_bytes;
	uint32_t data_in_size;
	uint32_t data_out_size;
	uint32_t max_sense_bytes;
	uint32_t data_sge_offset;
	uint8_t mf[1];
};

struct mpt3_ioctl_btdh_mapping {
	struct mpt3_ioctl_header hdr;
	uint32_t id;
	uint32_t bus;
	uint16_t handle;
	uint16_t rsvd;
};

/* IOCTL opcodes (subset) */
#define MPT3IOCINFO _IOWR(MPT3_MAGIC_NUMBER, 17, struct mpt3_ioctl_iocinfo)
#define MPT3COMMAND _IOWR(MPT3_MAGIC_NUMBER, 20, struct mpt3_ioctl_command)
#define MPT3BTDHMAPPING _IOWR(MPT3_MAGIC_NUMBER, 31, struct mpt3_ioctl_btdh_mapping)

#endif /* LINUX_MPSUTIL_MPT_IOCTL_H */

