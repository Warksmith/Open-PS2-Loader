// Original LUNA code: Danny Nunez (dnunezx) 2026
// Bound page I/O to validated file extents or the shared FHI transport.
#include "mcemu.h"

static int ioSema = -1;
static int (*bdRead)(u64, unsigned short, unsigned char *);
static int (*bdWrite)(u64, unsigned short, const unsigned char *);
static int (*fhiRead)(int, void *, unsigned int, unsigned int);
static int (*fhiWrite)(int, const void *, unsigned int, unsigned int);

int DeviceVmcInit(void)
{
    modinfo_t info;
    iop_sema_t sema = {0};
    int i;
    for (i = 0; i < MCEMU_PORTS; i++) {
        if (!vmcSpec[i].active)
            continue;
        if (vmcSpec[i].transport == 0) {
            if (!getModInfo("oplutils", &info) || !vmcSpec[i].frag_count ||
                vmcSpec[i].frag_count > 64)
                return 0;
            bdRead = (void *)info.exports[6];
            bdWrite = (void *)info.exports[7];
        } else if (vmcSpec[i].transport == 1) {
            if (!getModInfo("fhi\0\0\0\0\0", &info))
                return 0;
            fhiRead = (void *)info.exports[5];
            fhiWrite = (void *)info.exports[6];
        } else
            return 0;
    }
    sema.initial = 1;
    sema.max = 1;
    ioSema = CreateSema(&sema);
    return ioSema >= 0;
}

static int transfer(int slot, void *buffer, u32 page, int write)
{
    McImageSpec *card;
    u64 sector = 0;
    unsigned int i;
    int result = 0;
    if (slot < 0 || slot >= MCEMU_PORTS || ioSema < 0)
        return 0;
    card = &vmcSpec[slot];
    if (!card->active || page >= card->cspec.CardSize)
        return 0;
    if (card->transport == 0) {
        u32 offset = page;
        for (i = 0; i < card->frag_count; i++) {
            if (offset < card->frags[i].count) {
                sector = card->frags[i].sector + offset;
                break;
            }
            offset -= card->frags[i].count;
        }
        if (i == card->frag_count)
            return 0;
    }
    WaitSema(ioSema);
    if (card->transport == 1)
        result = write ? fhiWrite(4 + slot, buffer, page, 1) :
                         fhiRead(4 + slot, buffer, page, 1);
    else
        result = write ? bdWrite(sector, 1, buffer) : bdRead(sector, 1, buffer);
    SignalSema(ioSema);
    return result == 1;
}

int DeviceReadPage(int slot, void *buffer, u32 page) { return transfer(slot, buffer, page, 0); }
int DeviceWritePage(int slot, void *buffer, u32 page) { return transfer(slot, buffer, page, 1); }

void DeviceShutdown(void)
{
    // Freeze card I/O before CDVDMAN locks and flushes the block device.
    if (ioSema >= 0)
        WaitSema(ioSema);
}
