// Original LUNA code: Danny Nunez (dnunezx) 2026
// Connect OPL's disc emulator to the existing MMCE and UDPFS FHI ABI.
#include "internal.h"

static u32 (*fileSize)(int);
static int (*fileRead)(int, void *, unsigned int, unsigned int);
static int ioSema;

void DeviceInit(void)
{
    iop_sema_t sema = {0};
    sema.initial = 1;
    sema.max = 1;
    ioSema = CreateSema(&sema);
}

void DeviceFSInit(void)
{
    modinfo_t info;
    // OPL loads transport modules after the replacement IOPRP boots.
    while (!getModInfo("fhi\0\0\0\0\0", &info))
        DelayThread(100000);
    fileSize = (void *)info.exports[4];
    fileRead = (void *)info.exports[5];
    while (!fileSize(0))
        DelayThread(100000);
}

void DeviceDeinit(void) {}
int DeviceReady(void) { return fileSize && fileSize(0) ? SCECdComplete : SCECdNotReady; }
void DeviceLock(void) { WaitSema(ioSema); }
void DeviceUnmount(void) {}
void DeviceStop(void) {}

int DeviceReadSectors(u64 lsn, void *buffer, unsigned int sectors)
{
    int result;
    if (!fileRead)
        return SCECdErTRMOPN;
    if (lsn > 0x3fffffffu || sectors > 0x3fffffffu - (u32)lsn)
        return SCECdErREAD;
    WaitSema(ioSema);
    result = fileRead(0, buffer, (u32)lsn * 4, sectors * 4);
    SignalSema(ioSema);
    return result == (int)(sectors * 4) ? SCECdErNO : SCECdErREAD;
}
