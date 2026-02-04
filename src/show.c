/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mptutil.h"

#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MPT_MAX_UNIT 32

static const char *
dev_prefix(void)
{
	return g_ctx.is_mpt2 ? "mpt2ctl" : "mpt3ctl";
}

static const char *
get_device_type(uint32_t di)
{
	if (di & MPI2_SAS_DEVICE_INFO_SEP)
		return "SEP Target    ";
	if (di & MPI2_SAS_DEVICE_INFO_ATAPI_DEVICE)
		return "ATAPI Target  ";
	if (di & MPI2_SAS_DEVICE_INFO_SSP_TARGET)
		return "SAS Target    ";
	if (di & MPI2_SAS_DEVICE_INFO_STP_TARGET)
		return "STP Target    ";
	if (di & MPI2_SAS_DEVICE_INFO_SMP_TARGET)
		return "SMP Target    ";
	if (di & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
		return "SATA Target   ";
	if (di & (MPI2_SAS_DEVICE_INFO_SSP_INITIATOR |
		  MPI2_SAS_DEVICE_INFO_STP_INITIATOR |
		  MPI2_SAS_DEVICE_INFO_SMP_INITIATOR))
		return "SAS Initiator ";
	if (di & MPI2_SAS_DEVICE_INFO_SATA_HOST)
		return "SATA Initiator";
	if ((di & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE) == 0)
		return "No Device     ";
	return "Unknown Device";
}

static const char *
get_enc_type(uint16_t flags, int *issep)
{
	*issep = 0;
	switch (flags & 0xf) {
	case 0x01:
		*issep = 1;
		return "Direct Attached SES-2";
	case 0x02:
		return "Direct Attached SGPIO";
	case 0x03:
		return "Expander SGPIO";
	case 0x04:
		*issep = 1;
		return "External SES-2";
	case 0x05:
		return "Direct Attached GPIO";
	default:
		return "Unknown";
	}
}

static const char *mpt_device_speed[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"1.5", "3.0", "6.0", "12 ", /* indices 0x08..0x0b */
};

static const char *
get_device_speed(uint8_t rate)
{
	rate &= 0xf;
	if (rate >= (sizeof(mpt_device_speed) / sizeof(mpt_device_speed[0])))
		return "Unk";
	if (!mpt_device_speed[rate])
		return "???";
	return mpt_device_speed[rate];
}

static int
show_iocfacts(int fd, int unit)
{
	MPI2_IOC_FACTS_REPLY *facts = mpt_get_ioc_facts(fd, unit);
	if (!facts) {
		perror("IOC facts");
		return errno ? errno : EIO;
	}

	uint16_t msgver = le16toh(facts->MsgVersion);
	uint16_t headver = le16toh(facts->HeaderVersion);
	uint16_t ioc_status = le16toh(facts->IOCStatus);
	uint32_t ioc_loginfo = le32toh(facts->IOCLogInfo);
	uint32_t ioc_caps = le32toh(facts->IOCCapabilities);
	uint32_t fw = le32toh(facts->FWVersion.Word);

	printf("          MsgVersion: %u.%u\n", msgver >> 8, msgver & 0xff);
	printf("           MsgLength: %u\n", facts->MsgLength);
	printf("            Function: 0x%x\n", facts->Function);
	printf("       HeaderVersion: %u.%u\n", headver >> 8, headver & 0xff);
	printf("           IOCNumber: %u\n", facts->IOCNumber);
	printf("            MsgFlags: 0x%x\n", facts->MsgFlags);
	printf("               VP_ID: %u\n", facts->VP_ID);
	printf("               VF_ID: %u\n", facts->VF_ID);
	printf("       IOCExceptions: %u\n", le16toh(facts->IOCExceptions));
	printf("           IOCStatus: 0x%04x (%s)\n", ioc_status,
	    ioc_status_str(facts->IOCStatus));
	printf("          IOCLogInfo: 0x%08x\n", ioc_loginfo);
	printf("       MaxChainDepth: %u\n", facts->MaxChainDepth);
	printf("             WhoInit: 0x%x\n", facts->WhoInit);
	printf("       NumberOfPorts: %u\n", facts->NumberOfPorts);
	printf("      MaxMSIxVectors: %u\n", facts->MaxMSIxVectors);
	printf("       RequestCredit: %u\n", le16toh(facts->RequestCredit));
	printf("           ProductID: 0x%x\n", le16toh(facts->ProductID));
	printf("     IOCCapabilities: 0x%08x\n", ioc_caps);
	printf("           FWVersion: %u.%02u.%02u.%02u\n",
	    (fw >> 24) & 0xff, (fw >> 16) & 0xff, (fw >> 8) & 0xff, fw & 0xff);
	printf(" IOCRequestFrameSize: %u\n", le16toh(facts->IOCRequestFrameSize));
	printf("       MaxInitiators: %u\n", le16toh(facts->MaxInitiators));
	printf("          MaxTargets: %u\n", le16toh(facts->MaxTargets));
	printf("     MaxSasExpanders: %u\n", le16toh(facts->MaxSasExpanders));
	printf("       MaxEnclosures: %u\n", le16toh(facts->MaxEnclosures));
	printf("       ProtocolFlags: 0x%x\n", le16toh(facts->ProtocolFlags));
	printf("  HighPriorityCredit: %u\n", le16toh(facts->HighPriorityCredit));
	printf("MaxRepDescPostQDepth: %u\n",
	    le16toh(facts->MaxReplyDescriptorPostQueueDepth));
	printf("      ReplyFrameSize: %u\n", facts->ReplyFrameSize);
	printf("          MaxVolumes: %u\n", facts->MaxVolumes);
	printf("        MaxDevHandle: %u\n", le16toh(facts->MaxDevHandle));
	printf("MaxPersistentEntries: %u\n", le16toh(facts->MaxPersistentEntries));
	printf("        MinDevHandle: %u\n", le16toh(facts->MinDevHandle));

	free(facts);
	return 0;
}

static int
show_adapters(int fd)
{
	printf("Device\t\t      Chip Name        Board Name        Firmware\n");
	for (int unit = 0; unit < MPT_MAX_UNIT; unit++) {
		struct mpt3_ioctl_iocinfo info;
		if (mpt_ioctl_iocinfo(fd, unit, &info) < 0)
			continue;

		size_t man_len = 0;
		MPI2_CONFIG_PAGE_MAN_0 *man0 = mpt_read_config_page(fd, unit,
		    MPI2_CONFIG_PAGETYPE_MANUFACTURING, 0, 0, &man_len, NULL);
		if (!man0 || man_len < sizeof(*man0)) {
			/* Fall back to iocinfo only. */
			printf("%s:%d\t\t%16s %16s        %08x\n",
			    dev_prefix(), unit,
			    "?", "?", le32toh(info.firmware_version));
			free(man0);
			continue;
		}
		printf("%s:%d\t\t%16.16s %16.16s        %08x\n",
		    dev_prefix(), unit,
		    man0->ChipName, man0->BoardName, le32toh(info.firmware_version));
		free(man0);
	}
	return 0;
}

static int
show_adapter(int fd, int unit)
{
	int rc = 0;
	size_t len = 0;

	MPI2_CONFIG_PAGE_MAN_0 *man0 = mpt_read_config_page(fd, unit,
	    MPI2_CONFIG_PAGETYPE_MANUFACTURING, 0, 0, &len, NULL);
	if (!man0 || len < sizeof(*man0)) {
		fprintf(stderr, "Failed to read Manufacturing page 0\n");
		free(man0);
		return errno ? errno : EIO;
	}

	printf("%s Adapter %d:\n", dev_prefix(), unit);
	printf("       Board Name: %.16s\n", man0->BoardName);
	printf("   Board Assembly: %.16s\n", man0->BoardAssembly);
	printf("        Chip Name: %.16s\n", man0->ChipName);
	printf("    Chip Revision: %.16s\n", man0->ChipRevision);
	free(man0);

	MPI2_CONFIG_PAGE_BIOS_3 *bios3 = mpt_read_config_page(fd, unit,
	    MPI2_CONFIG_PAGETYPE_BIOS, 3, 0, &len, NULL);
	if (bios3 && len >= sizeof(*bios3)) {
		uint32_t v = le32toh(bios3->BiosVersion);
		printf("    BIOS Revision: %u.%02u.%02u.%02u\n",
		    (v >> 24) & 0xff, (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
	}
	free(bios3);

	MPI2_IOC_FACTS_REPLY *facts = mpt_get_ioc_facts(fd, unit);
	if (facts) {
		uint32_t fw = le32toh(facts->FWVersion.Word);
		uint32_t caps = le32toh(facts->IOCCapabilities);
		printf("Firmware Revision: %u.%02u.%02u.%02u\n",
		    (fw >> 24) & 0xff, (fw >> 16) & 0xff, (fw >> 8) & 0xff, fw & 0xff);
		printf("  Integrated RAID: %s\n",
		    (caps & MPI2_IOCFACTS_CAPABILITY_INTEGRATED_RAID) ? "yes" : "no");
		free(facts);
	} else {
		printf("Firmware Revision: (unavailable)\n");
	}

	MPI2_CONFIG_PAGE_IO_UNIT_1 *iounit1 = mpt_read_config_page(fd, unit,
	    MPI2_CONFIG_PAGETYPE_IO_UNIT, 1, 0, &len, NULL);
	if (iounit1 && len >= sizeof(*iounit1)) {
		uint32_t flags = le32toh(iounit1->Flags);
		printf("         SATA NCQ: %s\n",
		    (flags & MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE) ? "DISABLED" : "ENABLED");
	}
	free(iounit1);

	MPI2_CONFIG_PAGE_IO_UNIT_7 *iounit7 = mpt_read_config_page(fd, unit,
	    MPI2_CONFIG_PAGETYPE_IO_UNIT, 7, 0, &len, NULL);
	if (iounit7 && len >= sizeof(*iounit7)) {
		static const char *pcie_speed[] = { "2.5", "5.0", "8.0", "16.0", "32.0" };
		static const char *ioc_speeds[] = { "", "Full", "Half", "Quarter", "Eighth" };

		printf(" PCIe Width/Speed: x%u (%s GT/s)\n",
		    iounit7->PCIeWidth,
		    (iounit7->PCIeSpeed < (sizeof(pcie_speed)/sizeof(pcie_speed[0])))
		        ? pcie_speed[iounit7->PCIeSpeed] : "?");

		printf("        IOC Speed: %s\n",
		    (iounit7->IOCSpeed < (sizeof(ioc_speeds)/sizeof(ioc_speeds[0])))
		        ? ioc_speeds[iounit7->IOCSpeed] : "?");

		printf("      Temperature: ");
		if (iounit7->IOCTemperatureUnits == MPI2_IOUNITPAGE7_IOC_TEMP_NOT_PRESENT) {
			printf("Unknown/Unsupported\n");
		} else if (iounit7->IOCTemperatureUnits == MPI2_IOUNITPAGE7_IOC_TEMP_CELSIUS) {
			printf("%u C\n", le16toh(iounit7->IOCTemperature));
		} else if (iounit7->IOCTemperatureUnits == MPI2_IOUNITPAGE7_IOC_TEMP_FAHRENHEIT) {
			printf("%u F\n", le16toh(iounit7->IOCTemperature));
		} else {
			printf("%u (units=%u)\n", le16toh(iounit7->IOCTemperature),
			    iounit7->IOCTemperatureUnits);
		}
	}
	free(iounit7);

	/* PHY overview (SAS IO Unit pages 0/1 are extended config pages). */
	size_t sas0_len = 0, sas1_len = 0;
	MPI2_CONFIG_PAGE_SASIOUNIT_0 *sas0 = mpt_read_ext_config_page(fd, unit,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT, 0, MPI2_SASIOUNITPAGE0_PAGEVERSION, 0,
	    &sas0_len, NULL);
	MPI2_CONFIG_PAGE_SASIOUNIT_1 *sas1 = mpt_read_ext_config_page(fd, unit,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT, 1, MPI2_SASIOUNITPAGE1_PAGEVERSION, 0,
	    &sas1_len, NULL);

	if (sas0 && sas1 && sas0_len >= offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_0, PhyData) &&
	    sas1_len >= offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_1, PhyData)) {
		uint8_t num_phys = sas0->NumPhys;
		printf("\n");
		printf("%-8s%-12s%-11s%-10s%-8s%-7s%-7s%s\n",
		    "PhyNum", "CtlrHandle", "DevHandle", "Disabled",
		    "Speed", "Min", "Max", "Device");

		MPI2_SAS_IO_UNIT0_PHY_DATA *phy0 =
		    (MPI2_SAS_IO_UNIT0_PHY_DATA *)((uint8_t *)sas0 +
		    offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_0, PhyData));
		MPI2_SAS_IO_UNIT1_PHY_DATA *phy1 =
		    (MPI2_SAS_IO_UNIT1_PHY_DATA *)((uint8_t *)sas1 +
		    offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_1, PhyData));

		for (uint8_t i = 0; i < num_phys; i++) {
			const char *disabled = (phy0[i].PhyFlags & MPI2_SASIOUNIT0_PHYFLAGS_PHY_DISABLED) ? "Y" : "N";
			const char *speed = (le16toh(phy0[i].AttachedDevHandle) != 0) ? get_device_speed(phy0[i].NegotiatedLinkRate) : "     ";
			const char *min = get_device_speed(phy1[i].MaxMinLinkRate);
			const char *max = get_device_speed(phy1[i].MaxMinLinkRate >> 4);
			const char *dtype = get_device_type(le32toh(phy0[i].ControllerPhyDeviceInfo));

			if (phy0[i].PortFlags & MPI2_SASIOUNIT0_PORTFLAGS_DISCOVERY_IN_PROGRESS) {
				printf("%-8uDiscovery still in progress\n", i);
				continue;
			}

			if (le16toh(phy0[i].AttachedDevHandle) != 0) {
				printf("%-8u%04x        %04x       %-10s%-8s%-7s%-7s%s\n",
				    i,
				    le16toh(phy0[i].ControllerDevHandle),
				    le16toh(phy0[i].AttachedDevHandle),
				    disabled, speed, min, max, dtype);
			} else {
				printf("%-8u%04x        %-11s%-10s%-8s%-7s%-7s%s\n",
				    i,
				    le16toh(phy0[i].ControllerDevHandle),
				    "    ", disabled, speed, min, max, dtype);
			}
		}
	}
	free(sas0);
	free(sas1);

	return rc;
}

static int
show_devices(int fd, int unit)
{
	uint16_t ioc_status = 0;
	size_t sas0_len = 0;
	MPI2_CONFIG_PAGE_SASIOUNIT_0 *sas0 = mpt_read_ext_config_page(fd, unit,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT, 0, MPI2_SASIOUNITPAGE0_PAGEVERSION, 0,
	    &sas0_len, &ioc_status);
	uint8_t nphys = sas0 ? sas0->NumPhys : 0;

	MPI2_SAS_IO_UNIT0_PHY_DATA *phydata = NULL;
	if (sas0 && sas0_len >= offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_0, PhyData))
		phydata = (MPI2_SAS_IO_UNIT0_PHY_DATA *)((uint8_t *)sas0 +
		    offsetof(MPI2_CONFIG_PAGE_SASIOUNIT_0, PhyData));

	printf("B____%-5s%-17s%-8s%-10s%-14s%-6s%-5s%-6s%s\n",
	    "T", "SAS Address", "Handle", "Parent", "Device", "Speed",
	    "Enc", "Slot", "Ports");

	uint16_t handle = 0xffff;
	for (;;) {
		size_t dev_len = 0;
		MPI2_CONFIG_PAGE_SAS_DEV_0 *dev = mpt_read_ext_config_page(fd, unit,
		    MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE, 0, MPI2_SASDEVICE0_PAGEVERSION,
		    (uint32_t)(MPI2_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE | handle),
		    &dev_len, &ioc_status);
		if (!dev) {
			/* Most controllers signal end of list via invalid page. */
			break;
		}

		handle = le16toh(dev->DevHandle);

		if (dev->ParentDevHandle == 0x0) {
			free(dev);
			continue;
		}

		struct mpt3_ioctl_btdh_mapping map;
		memset(&map, 0, sizeof(map));
		map.bus = 0xFFFFFFFF;
		map.id = 0xFFFFFFFF;
		map.handle = handle;
		if (mpt_ioctl_btdh_mapping(fd, unit, &map) < 0) {
			/* Leave bus/target blank. */
			map.bus = 0xFFFFFFFF;
			map.id = 0xFFFFFFFF;
		}

		char bt[32];
		if (map.bus == 0xFFFFFFFF || map.id == 0xFFFFFFFF)
			snprintf(bt, sizeof(bt), "       ");
		else
			snprintf(bt, sizeof(bt), "%02u   %02u",
			    (unsigned)map.bus, (unsigned)map.id);

		const char *type = get_device_type(le32toh(dev->DeviceInfo));

		const char *speed = "";
		uint32_t devinfo = le32toh(dev->DeviceInfo);
		if (devinfo & MPI2_SAS_DEVICE_INFO_DIRECT_ATTACH) {
			if (phydata && dev->PhyNum < nphys)
				speed = get_device_speed(phydata[dev->PhyNum].NegotiatedLinkRate);
		} else if (le16toh(dev->ParentDevHandle) > 0) {
			size_t exp_len = 0;
			uint32_t addr = (uint32_t)(
			    MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
			    ((uint32_t)dev->PhyNum << MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT) |
			    le16toh(dev->ParentDevHandle));
			MPI2_CONFIG_PAGE_EXPANDER_1 *exp1 = mpt_read_ext_config_page(fd, unit,
			    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER, 1, MPI2_SASEXPANDER1_PAGEVERSION,
			    addr, &exp_len, NULL);
			if (exp1) {
				speed = get_device_speed(exp1->NegotiatedLinkRate);
				free(exp1);
			}
		}

		char sasaddr[17];
		snprintf(sasaddr, sizeof(sasaddr), "%08x%08x",
		    le32toh(dev->SASAddress.High), le32toh(dev->SASAddress.Low));

		char enchandle[8], slot[8];
		if (dev->EnclosureHandle != 0) {
			snprintf(enchandle, sizeof(enchandle), "%04x",
			    le16toh(dev->EnclosureHandle));
			snprintf(slot, sizeof(slot), "%02u", le16toh(dev->Slot));
		} else {
			snprintf(enchandle, sizeof(enchandle), "    ");
			snprintf(slot, sizeof(slot), "  ");
		}

		printf("%-10s%-17s%04x    %04x      %-14s%-6s%-5s%-6s%u\n",
		    bt, sasaddr,
		    le16toh(dev->DevHandle),
		    le16toh(dev->ParentDevHandle),
		    type, speed, enchandle, slot, dev->MaxPortConnections);

		free(dev);
	}

	free(sas0);
	printf("\n");
	return 0;
}

static int
show_enclosures(int fd, int unit)
{
	printf("Slots      Logical ID     SEPHandle  EncHandle    Type\n");

	uint16_t ioc_status = 0;
	uint16_t handle = 0xffff;
	for (;;) {
		size_t enc_len = 0;
		MPI2_CONFIG_PAGE_SAS_ENCLOSURE_0 *enc = mpt_read_ext_config_page(fd, unit,
		    MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE, 0, MPI2_SASENCLOSURE0_PAGEVERSION,
		    (uint32_t)(MPI2_SAS_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE | handle),
		    &enc_len, &ioc_status);
		if (!enc)
			break;

		int issep = 0;
		const char *type = get_enc_type(le16toh(enc->Flags), &issep);

		char sepstr[8];
		if (!issep)
			snprintf(sepstr, sizeof(sepstr), "    ");
		else
			snprintf(sepstr, sizeof(sepstr), "%04x", le16toh(enc->SEPDevHandle));

		printf("  %02u    %08x%08x    %s       %04x     %s\n",
		    le16toh(enc->NumSlots),
		    le32toh(enc->EnclosureLogicalID.High),
		    le32toh(enc->EnclosureLogicalID.Low),
		    sepstr,
		    le16toh(enc->EnclosureHandle),
		    type);

		handle = le16toh(enc->EnclosureHandle);
		free(enc);
	}
	printf("\n");
	return 0;
}

static int
show_expanders(int fd, int unit)
{
	printf("NumPhys   SAS Address     DevHandle   Parent  EncHandle  SAS Level\n");

	uint16_t ioc_status = 0;
	uint16_t handle = 0xffff;
	for (;;) {
		size_t exp0_len = 0;
		MPI2_CONFIG_PAGE_EXPANDER_0 *exp0 = mpt_read_ext_config_page(fd, unit,
		    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER, 0, MPI2_SASEXPANDER0_PAGEVERSION,
		    (uint32_t)(MPI2_SAS_EXPAND_PGAD_FORM_GET_NEXT_HNDL | handle),
		    &exp0_len, &ioc_status);
		if (!exp0)
			break;

		handle = le16toh(exp0->DevHandle);
		uint8_t nphys = exp0->NumPhys;

		char enchandle[8], parent[8];
		if (exp0->EnclosureHandle == 0x00)
			snprintf(enchandle, sizeof(enchandle), "    ");
		else
			snprintf(enchandle, sizeof(enchandle), "%04x", le16toh(exp0->EnclosureHandle));
		if (exp0->ParentDevHandle == 0x0)
			snprintf(parent, sizeof(parent), "    ");
		else
			snprintf(parent, sizeof(parent), "%04x", le16toh(exp0->ParentDevHandle));

		printf("  %02u    %08x%08x    %04x       %s     %s       %u\n",
		    exp0->NumPhys,
		    le32toh(exp0->SASAddress.High), le32toh(exp0->SASAddress.Low),
		    le16toh(exp0->DevHandle),
		    parent, enchandle, exp0->SASLevel);

		printf("\n");
		printf("     Phy  RemotePhy  DevHandle  Speed  Min   Max    Device\n");
		for (uint8_t i = 0; i < nphys; i++) {
			size_t exp1_len = 0;
			uint32_t addr = (uint32_t)(
			    MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
			    ((uint32_t)i << MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT) |
			    le16toh(exp0->DevHandle));
			MPI2_CONFIG_PAGE_EXPANDER_1 *exp1 = mpt_read_ext_config_page(fd, unit,
			    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER, 1, MPI2_SASEXPANDER1_PAGEVERSION,
			    addr, &exp1_len, NULL);
			if (!exp1)
				continue;

			const char *dtype = get_device_type(le32toh(exp1->AttachedDeviceInfo));
			const char *speed;
			char rphy[4], rhandle[8];
			if ((le32toh(exp1->AttachedDeviceInfo) & 0x7) == 0) {
				speed = "   ";
				snprintf(rphy, sizeof(rphy), "  ");
				snprintf(rhandle, sizeof(rhandle), "    ");
			} else {
				speed = get_device_speed(exp1->NegotiatedLinkRate);
				snprintf(rphy, sizeof(rphy), "%02u", exp1->AttachedPhyIdentifier);
				snprintf(rhandle, sizeof(rhandle), "%04x", le16toh(exp1->AttachedDevHandle));
			}

			const char *min = get_device_speed(exp1->HwLinkRate);
			const char *max = get_device_speed(exp1->HwLinkRate >> 4);

			printf("     %02u      %s        %s      %s   %s   %s   %s\n",
			    exp1->Phy, rphy, rhandle, speed, min, max, dtype);
			free(exp1);
		}

		printf("\n");
		free(exp0);
	}

	return 0;
}

static int
show_cfgpage(int fd, int unit, int argc, char **argv)
{
	uint32_t addr = 0;
	uint8_t page = 0;
	uint8_t num = 0;

	if (argc < 2) {
		fprintf(stderr, "show cfgpage: missing page\n");
		return EINVAL;
	}

	page = (uint8_t)strtoul(argv[1], NULL, 0);
	if (argc >= 3)
		num = (uint8_t)strtoul(argv[2], NULL, 0);
	if (argc >= 4)
		addr = (uint32_t)strtoul(argv[3], NULL, 0);

	void *data = NULL;
	size_t len = 0;
	uint16_t ioc_status = 0;

	if (page >= 0x10) {
		data = mpt_read_ext_config_page(fd, unit, page, num, 0, addr, &len, &ioc_status);
	} else {
		data = mpt_read_config_page(fd, unit, page, num, addr, &len, &ioc_status);
	}

	if (!data) {
		fprintf(stderr, "Failed to read cfg page (IOCStatus=%s)\n",
		    ioc_status_str(ioc_status));
		return errno ? errno : EIO;
	}

	hexdump(data, len, NULL);
	free(data);
	return 0;
}

static int
show_all(int fd, int unit)
{
	printf("Adapter:\n");
	(void)show_adapter(fd, unit);
	printf("Devices:\n");
	(void)show_devices(fd, unit);
	printf("Enclosures:\n");
	(void)show_enclosures(fd, unit);
	printf("Expanders:\n");
	(void)show_expanders(fd, unit);
	return 0;
}

static void
show_usage(void)
{
	fprintf(stderr,
	    "usage: %s [-u unit] show <command> [args...]\n"
	    "\n"
	    "show commands:\n"
	    "  adapter\n"
	    "  adapters\n"
	    "  all\n"
	    "  devices\n"
	    "  enclosures\n"
	    "  expanders\n"
	    "  iocfacts\n"
	    "  cfgpage <page> [num] [addr]\n",
	    dev_prefix());
}

int
cmd_show(int argc, char **argv)
{
	if (argc < 2) {
		show_usage();
		return EINVAL;
	}

	int fd = mpt_open(&g_ctx);
	if (fd < 0) {
		perror("open ioctl device");
		return errno;
	}

	const char *sub = argv[1];
	int rc = 0;

	if (strcmp(sub, "adapter") == 0) {
		rc = show_adapter(fd, g_ctx.unit);
	} else if (strcmp(sub, "adapters") == 0) {
		rc = show_adapters(fd);
	} else if (strcmp(sub, "all") == 0) {
		rc = show_all(fd, g_ctx.unit);
	} else if (strcmp(sub, "devices") == 0) {
		rc = show_devices(fd, g_ctx.unit);
	} else if (strcmp(sub, "enclosures") == 0) {
		rc = show_enclosures(fd, g_ctx.unit);
	} else if (strcmp(sub, "expanders") == 0) {
		rc = show_expanders(fd, g_ctx.unit);
	} else if (strcmp(sub, "iocfacts") == 0) {
		rc = show_iocfacts(fd, g_ctx.unit);
	} else if (strcmp(sub, "cfgpage") == 0) {
		rc = show_cfgpage(fd, g_ctx.unit, argc - 1, argv + 1);
	} else {
		fprintf(stderr, "Unknown show subcommand: %s\n", sub);
		show_usage();
		rc = EINVAL;
	}

	close(fd);
	return rc;
}
