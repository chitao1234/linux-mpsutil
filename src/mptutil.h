/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LINUX_MPSUTIL_MPTUTIL_H
#define LINUX_MPSUTIL_MPTUTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* MPI headers (copied from FreeBSD). */
#include "mpi/mpi2_type.h"
#include "mpi/mpi2.h"
#include "mpi/mpi2_cnfg.h"
#include "mpi/mpi2_ioc.h"
#include "mpi/mpi2_sas.h"

#include "mpt_ioctl.h"

#define MPTUTIL_VERSION "0.1.0"

struct mpt_ctx {
	bool is_mpt2;          /* true => /dev/mpt2ctl, false => /dev/mpt3ctl */
	int unit;              /* ioc_number */
	const char *devnode;   /* path to ioctl device */
};

extern struct mpt_ctx g_ctx;

/* Utility helpers */
void hexdump(const void *ptr, size_t len, const char *prefix);
const char *ioc_status_str(uint16_t ioc_status_le);
int mpt_parse_unit(const char *s, int *out_unit);
int mpt_parse_u8(const char *s, uint8_t *out);
int mpt_parse_u32(const char *s, uint32_t *out);

/* Device access */
int mpt_open(const struct mpt_ctx *ctx);

/* Raw mpt3sas ioctls */
int mpt_ioctl_iocinfo(int fd, int unit, struct mpt3_ioctl_iocinfo *info);
int mpt_ioctl_btdh_mapping(int fd, int unit, struct mpt3_ioctl_btdh_mapping *map);

/* Generic MPI command passthrough (MPT3COMMAND) */
int mpt_send_mpi(int fd,
	int unit,
	const void *req_bytes,
	size_t req_bytes_len,
	uint32_t data_sge_offset_dwords,
	void *reply_bytes,
	size_t reply_bytes_len,
	void *data_in,
	size_t data_in_len,
	const void *data_out,
	size_t data_out_len,
	unsigned timeout_sec);

/* Config page helpers (read-only) */
int mpt_read_config_page_header(int fd, int unit,
	uint8_t page_type, uint8_t page_number, uint32_t page_address,
	MPI2_CONFIG_PAGE_HEADER *out_hdr, uint16_t *out_ioc_status_le);

int mpt_read_ext_config_page_header(int fd, int unit,
	uint8_t ext_page_type, uint8_t page_number, uint8_t page_version, uint32_t page_address,
	MPI2_CONFIG_PAGE_HEADER *out_hdr, uint16_t *out_ext_len_le, uint16_t *out_ioc_status_le);

void *mpt_read_config_page(int fd, int unit,
	uint8_t page_type, uint8_t page_number, uint32_t page_address,
	size_t *out_len, uint16_t *out_ioc_status_le);

void *mpt_read_ext_config_page(int fd, int unit,
	uint8_t ext_page_type, uint8_t page_number, uint8_t page_version, uint32_t page_address,
	size_t *out_len, uint16_t *out_ioc_status_le);

/* IOC Facts */
MPI2_IOC_FACTS_REPLY *mpt_get_ioc_facts(int fd, int unit);

/* Show commands */
int cmd_show(int argc, char **argv);

#endif /* LINUX_MPSUTIL_MPTUTIL_H */

