# Third-Party Notices

This directory (`linux-mpsutil/`) contains a small userspace utility inspired
by FreeBSD's `mpsutil`/`mprutil` and built to work with the Linux `mpt3sas`
driver management ioctls.

This project is GPL-2.0-or-later (see `COPYING` and `LICENSE`). The third-party
components listed below retain their own licenses.

## Broadcom MPI / Fusion-MPT headers (BSD-3-Clause)

Files under `include/mpi/` are Broadcom/LSI MPI header definitions as vendored
by FreeBSD. Each file includes its own BSD-3-Clause-style license header.

Copyright (c) 2000-2020 Broadcom Inc. All rights reserved.

## Linux mpt3sas management ioctl ABI (GPL-2.0-or-later)

`src/mpt_ioctl.h` contains userspace declarations of ioctl numbers and structs
based on the Linux kernel driver's management interface:

- Linux kernel: `drivers/scsi/mpt3sas/mpt3sas_ctl.h`

That upstream file is licensed under the GNU General Public License, version 2
or (at your option) any later version.

## FreeBSD mpsutil/mprutil (credit)

This utility is inspired by the FreeBSD `mpsutil`/`mprutil` tools and aims to
provide a similar read-only "show" experience on Linux.
