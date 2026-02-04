/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mptutil.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *
baseprog(const char *argv0)
{
	const char *s = argv0 ? strrchr(argv0, '/') : NULL;
	return s ? s + 1 : (argv0 ? argv0 : "mptutil");
}

static void
usage(const char *argv0)
{
	const char *p = baseprog(argv0);
	fprintf(stderr,
	    "usage: %s [-u unit] [-d /dev/mpt{2,3}ctl] <command> ...\n"
	    "\n"
	    "commands:\n"
	    "  version\n"
	    "  show <subcommand> ...\n"
	    "\n"
	    "examples:\n"
	    "  %s show adapters\n"
	    "  %s -u 0 show adapter\n"
	    "  %s -u 0 show devices\n"
	    "  %s -u 0 show cfgpage 0x09 0\n",
	    p, p, p, p, p);
}

static int
cmd_version(void)
{
	printf("mptutil %s (%s)\n", MPTUTIL_VERSION,
	    g_ctx.is_mpt2 ? "mpt2ctl" : "mpt3ctl");
	return 0;
}

static void
ctx_init_from_argv0(const char *argv0)
{
	const char *p = baseprog(argv0);

	/*
	 * Match FreeBSD's UX: mpsutil vs mprutil are the same program, but
	 * pick the generation-specific control device.
	 */
	if (strcmp(p, "mprutil") == 0) {
		g_ctx.is_mpt2 = false;
		g_ctx.devnode = "/dev/" MPT3SAS_DEV_NAME;
	} else if (strcmp(p, "mpsutil") == 0) {
		g_ctx.is_mpt2 = true;
		g_ctx.devnode = "/dev/" MPT2SAS_DEV_NAME;
	} else {
		/* Default to mpt3ctl since it covers newer HBAs. */
		g_ctx.is_mpt2 = false;
		g_ctx.devnode = "/dev/" MPT3SAS_DEV_NAME;
	}
	g_ctx.unit = 0;
}

int
main(int argc, char **argv)
{
	const char *argv0 = argv[0];
	ctx_init_from_argv0(argv[0]);

	int ch;
	while ((ch = getopt(argc, argv, "u:d:h")) != -1) {
		switch (ch) {
		case 'u':
			g_ctx.unit = (int)strtol(optarg, NULL, 0);
			if (g_ctx.unit < 0) {
				fprintf(stderr, "Invalid unit: %s\n", optarg);
				return 1;
			}
			break;
		case 'd':
			g_ctx.devnode = optarg;
			/* Try to infer generation from device name. */
			if (strstr(optarg, MPT2SAS_DEV_NAME))
				g_ctx.is_mpt2 = true;
			else if (strstr(optarg, MPT3SAS_DEV_NAME))
				g_ctx.is_mpt2 = false;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage(argv0);
		return 1;
	}

	if (strcmp(argv[0], "version") == 0)
		return cmd_version();

	if (strcmp(argv[0], "show") == 0)
		return cmd_show(argc, argv);

	fprintf(stderr, "Unknown command: %s\n", argv[0]);
	usage(argv0);
	return 1;
}
