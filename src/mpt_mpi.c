/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mptutil.h"

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct mpt_ctx g_ctx;

int
mpt_open(const struct mpt_ctx *ctx)
{
	if (!ctx || !ctx->devnode) {
		errno = EINVAL;
		return -1;
	}
	return open(ctx->devnode, O_RDWR);
}

int
mpt_ioctl_iocinfo(int fd, int unit, struct mpt3_ioctl_iocinfo *info)
{
	if (!info) {
		errno = EINVAL;
		return -1;
	}
	memset(info, 0, sizeof(*info));
	info->hdr.ioc_number = (uint32_t)unit;
	if (ioctl(fd, MPT3IOCINFO, info) < 0)
		return -1;
	return 0;
}

int
mpt_ioctl_btdh_mapping(int fd, int unit, struct mpt3_ioctl_btdh_mapping *map)
{
	if (!map) {
		errno = EINVAL;
		return -1;
	}
	map->hdr.ioc_number = (uint32_t)unit;
	if (ioctl(fd, MPT3BTDHMAPPING, map) < 0)
		return -1;
	return 0;
}

static uint32_t
dwords_ceil(size_t bytes)
{
	return (uint32_t)((bytes + 3) / 4);
}

int
mpt_send_mpi(int fd,
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
	unsigned timeout_sec)
{
	size_t mf_len = (size_t)data_sge_offset_dwords * 4;
	size_t cmd_len;
	struct mpt3_ioctl_command *cmd;
	int rc;

	/* Kernel expects a reasonable SGE offset; enforce a minimum. */
	if (data_sge_offset_dwords == 0) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * The ioctl ABI encodes sizeof(struct mpt3_ioctl_command) (with mf[1]),
	 * but the kernel will read beyond that based on data_sge_offset.
	 */
	cmd_len = sizeof(*cmd) + mf_len - 1;
	cmd = calloc(1, cmd_len);
	if (!cmd)
		return -1;

	cmd->hdr.ioc_number = (uint32_t)unit;
	cmd->timeout = timeout_sec;
	cmd->reply_frame_buf_ptr = reply_bytes;
	cmd->data_in_buf_ptr = data_in;
	cmd->data_out_buf_ptr = (void *)data_out;
	cmd->sense_data_ptr = NULL;
	cmd->max_reply_bytes = (uint32_t)reply_bytes_len;
	cmd->data_in_size = (uint32_t)data_in_len;
	cmd->data_out_size = (uint32_t)data_out_len;
	cmd->max_sense_bytes = 0;
	cmd->data_sge_offset = data_sge_offset_dwords;

	if (req_bytes && req_bytes_len) {
		size_t to_copy = req_bytes_len < mf_len ? req_bytes_len : mf_len;
		memcpy(cmd->mf, req_bytes, to_copy);
	}

	rc = ioctl(fd, MPT3COMMAND, cmd);
	free(cmd);

	if (rc < 0)
		return -1;
	return 0;
}

int
mpt_read_config_page_header(int fd, int unit,
	uint8_t page_type, uint8_t page_number, uint32_t page_address,
	MPI2_CONFIG_PAGE_HEADER *out_hdr, uint16_t *out_ioc_status_le)
{
	MPI2_CONFIG_REQUEST req;
	MPI2_CONFIG_REPLY reply;
	uint32_t sge_off;

	memset(&req, 0, sizeof(req));
	memset(&reply, 0, sizeof(reply));

	req.Function = MPI2_FUNCTION_CONFIG;
	req.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	req.Header.PageType = page_type;
	req.Header.PageNumber = page_number;
	req.Header.PageVersion = 0;
	req.Header.PageLength = 0;
	req.PageAddress = htole32(page_address);

	sge_off = (uint32_t)(offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE) / 4);
	if (mpt_send_mpi(fd, unit, &req, offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE),
		sge_off, &reply, sizeof(reply), NULL, 0, NULL, 0, 30) < 0)
		return -1;

	if (out_ioc_status_le)
		*out_ioc_status_le = reply.IOCStatus;
	if (out_hdr)
		*out_hdr = reply.Header;

	/* Treat non-success as an error. */
	if ((le16toh(reply.IOCStatus) & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		errno = EIO;
		return -1;
	}

	return 0;
}

int
mpt_read_ext_config_page_header(int fd, int unit,
	uint8_t ext_page_type, uint8_t page_number, uint8_t page_version, uint32_t page_address,
	MPI2_CONFIG_PAGE_HEADER *out_hdr, uint16_t *out_ext_len_le, uint16_t *out_ioc_status_le)
{
	MPI2_CONFIG_REQUEST req;
	MPI2_CONFIG_REPLY reply;
	uint32_t sge_off;

	memset(&req, 0, sizeof(req));
	memset(&reply, 0, sizeof(reply));

	req.Function = MPI2_FUNCTION_CONFIG;
	req.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	req.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	req.ExtPageType = ext_page_type;
	req.Header.PageNumber = page_number;
	req.Header.PageVersion = page_version;
	req.Header.PageLength = 0;
	req.PageAddress = htole32(page_address);

	sge_off = (uint32_t)(offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE) / 4);
	if (mpt_send_mpi(fd, unit, &req, offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE),
		sge_off, &reply, sizeof(reply), NULL, 0, NULL, 0, 30) < 0)
		return -1;

	if (out_ioc_status_le)
		*out_ioc_status_le = reply.IOCStatus;
	if (out_hdr)
		*out_hdr = reply.Header;
	if (out_ext_len_le)
		*out_ext_len_le = reply.ExtPageLength;

	if ((le16toh(reply.IOCStatus) & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		errno = EIO;
		return -1;
	}

	return 0;
}

void *
mpt_read_config_page(int fd, int unit,
	uint8_t page_type, uint8_t page_number, uint32_t page_address,
	size_t *out_len, uint16_t *out_ioc_status_le)
{
	MPI2_CONFIG_PAGE_HEADER hdr;
	MPI2_CONFIG_REQUEST req;
	MPI2_CONFIG_REPLY reply;
	uint32_t sge_off;
	size_t len;
	void *buf;

	memset(&hdr, 0, sizeof(hdr));
	if (mpt_read_config_page_header(fd, unit, page_type, page_number,
		page_address, &hdr, out_ioc_status_le) < 0)
		return NULL;

	if (hdr.PageLength == 0)
		hdr.PageLength = 4;
	len = (size_t)hdr.PageLength * 4;

	buf = calloc(1, len);
	if (!buf)
		return NULL;

	memset(&req, 0, sizeof(req));
	memset(&reply, 0, sizeof(reply));

	req.Function = MPI2_FUNCTION_CONFIG;
	req.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	req.Header = hdr;
	req.PageAddress = htole32(page_address);

	sge_off = (uint32_t)(offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE) / 4);
	if (mpt_send_mpi(fd, unit, &req, offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE),
		sge_off, &reply, sizeof(reply), buf, len, NULL, 0, 30) < 0) {
		free(buf);
		return NULL;
	}

	if (out_ioc_status_le)
		*out_ioc_status_le = reply.IOCStatus;

	if ((le16toh(reply.IOCStatus) & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		free(buf);
		errno = EIO;
		return NULL;
	}

	if (out_len)
		*out_len = len;
	return buf;
}

void *
mpt_read_ext_config_page(int fd, int unit,
	uint8_t ext_page_type, uint8_t page_number, uint8_t page_version, uint32_t page_address,
	size_t *out_len, uint16_t *out_ioc_status_le)
{
	MPI2_CONFIG_PAGE_HEADER hdr;
	MPI2_CONFIG_REQUEST req;
	MPI2_CONFIG_REPLY reply;
	uint16_t ext_len_le;
	uint32_t sge_off;
	size_t len;
	void *buf;

	memset(&hdr, 0, sizeof(hdr));
	ext_len_le = 0;
	if (mpt_read_ext_config_page_header(fd, unit, ext_page_type, page_number,
		page_version, page_address, &hdr, &ext_len_le, out_ioc_status_le) < 0)
		return NULL;

	if (le16toh(ext_len_le) == 0)
		ext_len_le = htole16(4);
	len = (size_t)le16toh(ext_len_le) * 4;

	buf = calloc(1, len);
	if (!buf)
		return NULL;

	memset(&req, 0, sizeof(req));
	memset(&reply, 0, sizeof(reply));

	req.Function = MPI2_FUNCTION_CONFIG;
	req.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	req.Header = hdr;
	req.ExtPageType = ext_page_type;
	req.ExtPageLength = ext_len_le;
	req.PageAddress = htole32(page_address);

	sge_off = (uint32_t)(offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE) / 4);
	if (mpt_send_mpi(fd, unit, &req, offsetof(MPI2_CONFIG_REQUEST, PageBufferSGE),
		sge_off, &reply, sizeof(reply), buf, len, NULL, 0, 30) < 0) {
		free(buf);
		return NULL;
	}

	if (out_ioc_status_le)
		*out_ioc_status_le = reply.IOCStatus;

	if ((le16toh(reply.IOCStatus) & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		free(buf);
		errno = EIO;
		return NULL;
	}

	if (out_len)
		*out_len = len;
	return buf;
}

MPI2_IOC_FACTS_REPLY *
mpt_get_ioc_facts(int fd, int unit)
{
	MPI2_IOC_FACTS_REQUEST req;
	MPI2_IOC_FACTS_REPLY *facts;
	uint32_t sge_off;

	memset(&req, 0, sizeof(req));
	req.Function = MPI2_FUNCTION_IOC_FACTS;

	/*
	 * IOC Facts has no SGL; provide a safe offset past the request struct
	 * so the kernel can build a zero-length SGE there.
	 */
	sge_off = dwords_ceil(sizeof(req));

	facts = calloc(1, 256);
	if (!facts)
		return NULL;

	if (mpt_send_mpi(fd, unit, &req, sizeof(req), sge_off,
		facts, 256, NULL, 0, NULL, 0, 10) < 0) {
		free(facts);
		return NULL;
	}

	/* Sanity: firmware returns MsgLength in dwords. */
	size_t facts_len = (size_t)facts->MsgLength * 4;
	if (facts_len > 4096 || facts_len < sizeof(MPI2_IOC_FACTS_REPLY)) {
		/* Keep what we have; caller might still print basics. */
		return facts;
	}

	/* If reply is bigger, re-issue with a larger buffer. */
	if (facts_len > 256) {
		MPI2_IOC_FACTS_REPLY *facts2 = calloc(1, facts_len);
		if (!facts2) {
			/* Return the smaller one rather than failing. */
			return facts;
		}
		if (mpt_send_mpi(fd, unit, &req, sizeof(req), sge_off,
			facts2, facts_len, NULL, 0, NULL, 0, 10) < 0) {
			free(facts2);
			return facts;
		}
		free(facts);
		facts = facts2;
	}

	return facts;
}

