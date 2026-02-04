# linux-mpsutil (mpsutil/mprutil for Linux)

This is a small, read-only management utility for Broadcom/LSI "Fusion-MPT"
SAS controllers on Linux, inspired by FreeBSD's `mpsutil`/`mprutil`.

It talks to the in-kernel `mpt3sas` driver via the existing misc ioctl devices:

- SAS2 / MPI2 generation: `/dev/mpt2ctl` (run as `mpsutil`)
- SAS3+ / MPI2.5/2.6 generation: `/dev/mpt3ctl` (run as `mprutil`)

Right now it focuses on `show` (read-only) commands only.

## Build

```sh
make -C linux-mpsutil
```

This produces:
- `linux-mpsutil/mpsutil`
- `linux-mpsutil/mprutil` (symlink to `mpsutil`, argv[0] controls which device is used)

## Usage

Examples:

```sh
sudo ./linux-mpsutil/mpsutil show adapters
sudo ./linux-mpsutil/mpsutil -u 0 show adapter
sudo ./linux-mpsutil/mpsutil -u 0 show devices

sudo ./linux-mpsutil/mprutil show adapters
sudo ./linux-mpsutil/mprutil -u 0 show iocfacts

# Dump raw config page bytes (page [num] [addr])
sudo ./linux-mpsutil/mpsutil -u 0 show cfgpage 0x09 0
```

Notes:
- `-u` selects `ioc_number` within the chosen generation class (mpt2 vs mpt3).
  The kernel assigns these IDs independently for SAS2 vs SAS3 HBAs.
- You typically need root (or appropriate permissions) to open `/dev/mpt2ctl` or
  `/dev/mpt3ctl`.

## Implemented show commands

- `show adapter`
- `show adapters`
- `show all`
- `show devices`
- `show enclosures`
- `show expanders`
- `show iocfacts`
- `show cfgpage <page> [num] [addr]` (hex dump)

## Implementation notes

- MPI message structs and config page definitions are from FreeBSD's MPI headers
  (BSD-licensed) and are vendored under `include/mpi/`.
- The ioctl ABI structs are a minimal userspace copy of the Linux `mpt3sas`
  management interface (see `src/mpt_ioctl.h`).

## License

`linux-mpsutil` is licensed under the GNU General Public License, version 2 or
(at your option) any later version (GPL-2.0-or-later). See `COPYING` and
`LICENSE`.

Vendored third-party components are listed in `THIRD_PARTY_NOTICES.md`. In
particular, the MPI headers under `include/mpi/` are BSD-licensed and retain
their original license headers.
