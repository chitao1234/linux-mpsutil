/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mptutil.h"

#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static const char *ioc_status_codes[] = {
	"Success", /* 0x0000 */
	"Invalid function",
	"Busy",
	"Invalid scatter-gather list",
	"Internal error",
	"Reserved",
	"Insufficient resources",
	"Invalid field",
	"Invalid state", /* 0x0008 */
	"Operation state not supported",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* 0x0010 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* 0x0018 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"Invalid configuration action", /* 0x0020 */
	"Invalid configuration type",
	"Invalid configuration page",
	"Invalid configuration data",
	"No configuration defaults",
	"Unable to commit configuration change",
};

const char *
ioc_status_str(uint16_t ioc_status_le)
{
	static char buf[32];
	uint16_t st = le16toh(ioc_status_le) & MPI2_IOCSTATUS_MASK;

	if (st < (sizeof(ioc_status_codes) / sizeof(ioc_status_codes[0])) &&
	    ioc_status_codes[st])
		return ioc_status_codes[st];

	snprintf(buf, sizeof(buf), "0x%04x", st);
	return buf;
}

#define MPT_PCHAR(_c) \
	do { \
		if (retval + 1 < outbuf_sz) { \
			outbuf[retval] = (_c); \
			retval++; \
		} \
	} while (0)

size_t
mpt_parse_flags(uintmax_t num, const char *q, char *outbuf, size_t outbuf_sz)
{
	const unsigned char *p = (const unsigned char *)q;
	size_t retval = 0;
	size_t base;

	if (!outbuf || outbuf_sz == 0)
		return 0;

	outbuf[0] = '\0';
	if (!q || num == 0)
		return 0;

	/* FreeBSD-style %b conversion flag format. */
	base = retval;
	while (*p) {
		unsigned int bit = *p++;

		if (bit > 0 && bit <= (sizeof(num) * 8) &&
		    (num & ((uintmax_t)1 << (bit - 1)))) {
			MPT_PCHAR(retval != base ? ',' : '<');
			while (*p > ' ') {
				MPT_PCHAR((char)*p);
				p++;
			}
		} else {
			while (*p > ' ')
				p++;
		}
	}
	if (retval != base)
		MPT_PCHAR('>');

	outbuf[retval < outbuf_sz ? retval : (outbuf_sz - 1)] = '\0';
	return retval;
}

void
hexdump(const void *ptr, size_t len, const char *prefix)
{
	const unsigned char *p = (const unsigned char *)ptr;

	for (size_t off = 0; off < len; off += 16) {
		if (prefix)
			printf("%s", prefix);
		printf("%04zx  ", off);

		for (size_t i = 0; i < 16; i++) {
			if (off + i < len)
				printf(" %02x", p[off + i]);
			else
				printf("   ");
		}

		printf("  |");
		for (size_t i = 0; i < 16; i++) {
			if (off + i >= len) {
				printf(" ");
				continue;
			}
			unsigned char c = p[off + i];
			printf("%c", isprint(c) ? c : '.');
		}
		printf("|\n");
	}
}

int
mpt_parse_unit(const char *s, int *out_unit)
{
	char *end = NULL;
	long v;

	if (!s || !out_unit) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	v = strtol(s, &end, 0);
	if (errno != 0 || end == s || !end || *end != '\0' || v < 0 || v > INT_MAX) {
		errno = EINVAL;
		return -1;
	}

	*out_unit = (int)v;
	return 0;
}

int
mpt_parse_u8(const char *s, uint8_t *out)
{
	char *end = NULL;
	unsigned long v;

	if (!s || !out) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	v = strtoul(s, &end, 0);
	if (errno != 0 || end == s || !end || *end != '\0' || v > UINT8_MAX) {
		errno = EINVAL;
		return -1;
	}

	*out = (uint8_t)v;
	return 0;
}

int
mpt_parse_u32(const char *s, uint32_t *out)
{
	char *end = NULL;
	unsigned long v;

	if (!s || !out) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	v = strtoul(s, &end, 0);
	if (errno != 0 || end == s || !end || *end != '\0' || v > UINT32_MAX) {
		errno = EINVAL;
		return -1;
	}

	*out = (uint32_t)v;
	return 0;
}
