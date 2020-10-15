/*
 * Updater.c
 *
 *  Created on: 4 août 2020
 *      Author: WidedCherif
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Updater.h"
#include "inc/hw_flash.h"
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/udma.h"
#include "usblib/usblib.h"
#include "usblib/usbmsc.h"
#include "usblib/host/usbhost.h"
#include "usblib/host/usbhmsc.h"
#include "BswComponentManager.h"
#include "BasicTypes.h"
#if defined(ewarm)
#pragma data_alignment=1024
tDMAControlTable g_sDMAControlTable[6];
#elif defined(ccs)
#pragma DATA_ALIGN(g_sDMAControlTable, 1024)
tDMAControlTable g_sDMAControlTable[6];
#else
tDMAControlTable g_sDMAControlTable[6] __attribute__ ((aligned(1024)));
#endif


#if defined(ccs) ||                                                           \
    defined(codered) ||                                                       \
    defined(gcc) ||                                                           \
    defined(rvmdk) ||                                                         \
    defined(__ARMCC_VERSION) ||                                               \
    defined(sourcerygxx)
//#define PACKED                  __attribute__((packed))
#elif defined(ewarm)
#define PACKED
#else
#error "Unrecognized COMPILER!"
#endif


extern const tUSBHostClassDriver g_sUSBHostMSCClassDriver;
static tUSBHostClassDriver const * const g_ppHostClassDrivers[] =
{
    &g_sUSBHostMSCClassDriver
};

tUSBHMSCInstance *g_psMSCInstance = 0;
#define HCD_MEMORY_SIZE         128
U8 g_pHCDPool[HCD_MEMORY_SIZE];
static U8 g_ui8SectorBuf[512];
U32 g_ui32SysClock;
static U8 *g_pui8SectorBuf;
#define FLASH_SIZE (1 * 1024 * 1024)
#define NUM_CLASS_DRIVERS       (sizeof(g_ppHostClassDrivers) /               \
                                 sizeof(g_ppHostClassDrivers[0]))

volatile enum
{
    STATE_NO_DEVICE,
    STATE_DEVICE_ENUM,
}
g_eState;

#ifdef ewarm
#pragma pack(1)
#endif

typedef struct
{
    U8 ui8DriveNumber;
    U8 ui8Reserved;
    U8 ui8ExtSig;
    U32 ui32Serial;
    Char pcVolumeLabel[11];
    Char pcFsType[8];
    U8 ui8BootCode[448];
    U16 ui16Sig;
}
PACKED tBootExt16;

typedef struct
{
    U32 ui32SectorsPerFAT;
    U16 ui16Flags;
    U16 ui16Version;
    U32 ui32RootCluster;
    U16 ui16InfoSector;
    U16 ui16BootCopy;
    U8 ui8Reserved[12];
    U8 ui8DriveNumber;
    U8 ui8Reserved1;
    U8 ui8ExtSig;
    U32 ui32Serial;
    Char pcVolumeLabel[11];
    Char pcFsType[8];
    U8 ui8BootCode[420];
    U16 ui16Sig;
}
PACKED tBootExt32;

typedef struct
{
    U8 ui8Status;
    U8 ui8CHSFirst[3];
    U8 ui8Type;
    U8 ui8CHSLast[3];
    U32 ui32FirstSector;
    U32 ui32NumBlocks;
}
PACKED tPartitionTable;
typedef struct
{
    U8 ui8CodeArea[440];
    U8 ui8DiskSignature[4];
    U8 ui8Nulls[2];
    tPartitionTable sPartTable[4];
    U16 ui16Sig;
}
PACKED tMasterBootRecord;

typedef struct
{
    U8 ui8Jump[3];
    U8 i8OEMName[8];
    U16 ui16BytesPerSector;
    U8 ui8SectorsPerCluster;
    U16 ui16ReservedSectors;
    U8 ui8NumFATs;
    U16 ui16NumRootEntries;
    U16 ui16TotalSectorsSmall;
    U8 ui8MediaDescriptor;
    U16 ui16SectorsPerFAT;
    U16 ui16SectorsPerTrack;
    U16 ui16NumberHeads;
    U32 ui32HiddenSectors;
    U32 ui32TotalSectorsBig;
    union
    {
        tBootExt16 sExt16;
        tBootExt32 sExt32;
    }
    PACKED ext;
}
PACKED tBootSector;
typedef struct
{   Char    pcFileName[11];
    U8 ui8Attr;
    U8 ui8Reserved;
    U8 ui8CreateTime[5];
    U8 ui8LastDate[2];
    U16 ui16ClusterHi;
    U8 ui8LastModified[4];
    U16 ui16Cluster;
    U32 ui32FileSize;
}
PACKED tDirEntry ;

#ifdef ewarm
#pragma pack()
#endif

typedef struct
{
    U32 ui32FirstSector;
    U32 ui32NumBlocks;
    U16 ui16SectorsPerCluster;
    U16 ui16MaxRootEntries;
    U32 ui32SectorsPerFAT;
    U32 ui32FirstFATSector;
    U32 ui32LastFATSector;
    U32 ui32FirstDataSector;
    U32 ui32Type;
    U32 ui32StartRootDir;
}
tPartitionInfo;

static tPartitionInfo sPartInfo;

typedef struct updater_struct {
       BswComponent Component;
       UpdaterConfig* Config;
}updater;

static void CallbacksfromMSDevice(tUSBHMSCInstance *psMSCInstance,U32 ui32Event, void *pvData);
void Config_usb() ;
U32 Init_file(U8 *pui8SectorBuf);
U32 GetNextCluster(UF32 ui32ThisCluster);
U32 ReadMediaSector(UF32 ui32Sector, U8 *pui8Buf);
U32 GetNextFileSector(UF32 ui32StartCluster);
U32 Open_file(update object);
U32 ReadAppAndProgram(update object);
void UpdaterUSB(update object);
extern void Updater_main() ;


update Updater_createInstance(const Char* name, UpdaterConfig* config)
{
    updater* instance = (updater*)Mem_alloc(sizeof(updater));
       if(instance != NULL)
       {
            instance->Config = config;
    (void)BswComponentManager_onComponentCreated((BswComponent*)instance, config, name, "Updater", &Updater_init, &Updater_start, &Updater_stop, false, false);

         }
    return instance;
}
Bool Updater_init () {
 config_usb();
return 0;
}
void Updater_start(update object) {
    updater* instance = (updater*)object;
       while(1)
          {
              USBHCDMain();

              switch(g_eState)
              {
                  case STATE_DEVICE_ENUM:
                  {
                      if(ReadAppAndProgram(instance))
                      {
                          g_eState = STATE_NO_DEVICE;
                      }
                      else
                      {
                          HWREG(NVIC_APINT) = NVIC_APINT_VECTKEY |
                                              NVIC_APINT_SYSRESETREQ;

                          while(1)
                          {
                          }
                      }
                      break;
                  }
                  case STATE_NO_DEVICE:
                  {
                      break;
                  }
              }
          }
}


Bool Updater_stop() {
return 0;
}
extern void Updater_main(update object){
    updater* instance = (updater*)object;
      Updater_init();
      Updater_start(instance);
}
static void CallbacksfromMSDevice(tUSBHMSCInstance *psMSCInstance,U32 ui32Event, void *pvData){
    switch(ui32Event){
    case MSC_EVENT_OPEN:
           {
               g_eState = STATE_DEVICE_ENUM;
               break;
           }
    case MSC_EVENT_CLOSE:
            {
                g_eState = STATE_NO_DEVICE;
                break;
            }

            default:
            {
                break;
            }
    }
}
U32 ReadMediaSector(UF32 ui32Sector, U8 *pui8Buf)
{
    return(USBHMSCBlockRead(g_psMSCInstance, ui32Sector, pui8Buf, 1));
}
U32 Init_file(U8 *pui8SectorBuf){
        tMasterBootRecord *pMBR;
        tPartitionTable *pPart;
        tBootSector *pBoot;

        g_pui8SectorBuf = pui8SectorBuf;

        if(ReadMediaSector(0, pui8SectorBuf))
        {
            return(1);
        }

        pMBR = (tMasterBootRecord *)pui8SectorBuf;
        if(pMBR->ui16Sig != 0xAA55)
        {
            return(1);
        }

        pBoot = (tBootSector *)pui8SectorBuf;
        if((strncmp(pBoot->ext.sExt16.pcFsType, "FAT", 3) != 0) &&
           (strncmp(pBoot->ext.sExt32.pcFsType, "FAT32", 5) != 0))
        {
            pPart = &(pMBR->sPartTable[0]);
            sPartInfo.ui32FirstSector = pPart->ui32FirstSector;
            sPartInfo.ui32NumBlocks = pPart->ui32NumBlocks;
            if(ReadMediaSector(sPartInfo.ui32FirstSector, pui8SectorBuf))
            {
                return(1);
            }
        }
        else
        {
            sPartInfo.ui32FirstSector = 0;
            if(pBoot->ui16TotalSectorsSmall == 0)
            {
                sPartInfo.ui32NumBlocks = pBoot->ui32TotalSectorsBig;
            }
            else
            {
                sPartInfo.ui32NumBlocks = pBoot->ui16TotalSectorsSmall;
            }
        }
        if(pBoot->ext.sExt16.ui16Sig != 0xAA55)
        {
            return(1);
        }
        if(pBoot->ui16BytesPerSector != 512)
        {
            return(1);
        }
        sPartInfo.ui16SectorsPerCluster = pBoot->ui8SectorsPerCluster;
        sPartInfo.ui16MaxRootEntries = pBoot->ui16NumRootEntries;
        if(sPartInfo.ui16MaxRootEntries == 0)
        {
            if(!strncmp(pBoot->ext.sExt32.pcFsType, "FAT32   ", 8))
            {
                sPartInfo.ui32Type = 32;
            }
            else
            {
                return(1);
            }
        }
        else
        {
            if(!strncmp(pBoot->ext.sExt16.pcFsType, "FAT16   ", 8))
            {
                sPartInfo.ui32Type = 16;
            }
            else
            {
                return(1);
            }
        }

        sPartInfo.ui32FirstFATSector = sPartInfo.ui32FirstSector +
                                     pBoot->ui16ReservedSectors;
        sPartInfo.ui32SectorsPerFAT = (sPartInfo.ui32Type == 16) ?
                                    pBoot->ui16SectorsPerFAT :
                                    pBoot->ext.sExt32.ui32SectorsPerFAT;
        sPartInfo.ui32LastFATSector = sPartInfo.ui32FirstFATSector +
                                    sPartInfo.ui32SectorsPerFAT - 1;
        if(sPartInfo.ui32Type == 16)
        {
            sPartInfo.ui32StartRootDir = sPartInfo.ui32FirstFATSector +
                                       (sPartInfo.ui32SectorsPerFAT *
                                        pBoot->ui8NumFATs);
            sPartInfo.ui32FirstDataSector = sPartInfo.ui32StartRootDir +
                                          (sPartInfo.ui16MaxRootEntries / 16);
        }
        else
        {
            sPartInfo.ui32StartRootDir = pBoot->ext.sExt32.ui32RootCluster;
            sPartInfo.ui32FirstDataSector = sPartInfo.ui32FirstFATSector +
                                (sPartInfo.ui32SectorsPerFAT * pBoot->ui8NumFATs);
        }
        return(0);
}

U32 GetNextCluster(UF32 ui32ThisCluster)
{
    static U8 ui8FATCache[512];
    static UF32 ui32CachedFATSector = (U32)-1;
    UF32 ui32ClustersPerFATSector;
    UF32 ui32ClusterIdx;
    UF32 ui32FATSector;
    UF32 ui32NextCluster;
    UF32 ui32MaxCluster;
    ui32MaxCluster = sPartInfo.ui32NumBlocks / sPartInfo.ui16SectorsPerCluster;
    if((ui32ThisCluster < 2) || (ui32ThisCluster > ui32MaxCluster))
    {
        return(0);
    }
    ui32ClustersPerFATSector = (sPartInfo.ui32Type == 16) ? 256 : 128;
    ui32ClusterIdx = ui32ThisCluster % ui32ClustersPerFATSector;
    ui32FATSector = ui32ThisCluster / ui32ClustersPerFATSector;
    if(ui32FATSector != ui32CachedFATSector)
    {
        if(ReadMediaSector(sPartInfo.ui32FirstFATSector + ui32FATSector,
                                 ui8FATCache) != 0)
        {
            ui32CachedFATSector = (U32)-1;
            return(0);
        }
        ui32CachedFATSector = ui32FATSector;
    }
    if(sPartInfo.ui32Type == 16)
    {
        ui32NextCluster = ((U16 *)ui8FATCache)[ui32ClusterIdx];
        if(ui32NextCluster >= 0xFFF8)
        {
            return(0);
        }
    }
    else
    {
        ui32NextCluster = ((U32 *)ui8FATCache)[ui32ClusterIdx];
        if(ui32NextCluster >= 0x0FFFFFF8)
        {
            return(0);
        }
    }
    if((ui32NextCluster >= 2) && (ui32NextCluster <= ui32MaxCluster))
    {
        return(ui32NextCluster);
    }
    else
    {
        return(0);
    }
}
U32 GetNextFileSector(UF32 ui32StartCluster)
{
    static UF32 ui32WorkingCluster = 0;
    static UF32 ui32WorkingSector;
    UF32 ui32ReadSector;
    if(ui32StartCluster)
    {
        ui32WorkingCluster = ui32StartCluster;
        ui32WorkingSector = 0;
        return(0);
    }
    else if(ui32WorkingCluster == 0)
    {
        return(0);
    }
    if(ui32WorkingSector == sPartInfo.ui16SectorsPerCluster)
    {
        ui32WorkingCluster = GetNextCluster(ui32WorkingCluster);
        if(ui32WorkingCluster)
        {
            ui32WorkingSector = 0;
        }
        else
        {
            ui32WorkingCluster = 0;
            return(0);
        }
    }
    ui32ReadSector = (ui32WorkingCluster - 2) * sPartInfo.ui16SectorsPerCluster;
    ui32ReadSector += ui32WorkingSector;
    ui32ReadSector += sPartInfo.ui32FirstDataSector;
    if(ReadMediaSector(ui32ReadSector, g_pui8SectorBuf) != 0)
    {
        ui32WorkingCluster = 0;
        return(0);
    }
    else
    {
        ui32WorkingSector++;
        return(1);
    }
}
U32 Open_file(update object){
        updater* instance = (updater*)object;
        tDirEntry *pDirEntry;
        UF32 ui32DirSector;
        UF32 ui32FirstCluster;
        ui32DirSector = sPartInfo.ui32StartRootDir;
        if(sPartInfo.ui32Type == 32)
        {
            GetNextFileSector(ui32DirSector);
        }
        while(1)
        {
            if(sPartInfo.ui32Type == 16)
            {
                if(ReadMediaSector(ui32DirSector, g_pui8SectorBuf))
                {
                    return(0);
                }
            }
            else
            {
                if(GetNextFileSector(0) == 0)
                {
                    return(0);
                }
            }
            pDirEntry= (tDirEntry *)g_pui8SectorBuf;
            while((U8 *)(pDirEntry) < &g_pui8SectorBuf[512])
            {
                if(!strncmp(pDirEntry->pcFileName,instance->Config->FileName,instance->Config->namesize))
                {
                    ui32FirstCluster = pDirEntry->ui16Cluster;
                    if(sPartInfo.ui32Type == 32)
                    {
                        ui32FirstCluster += pDirEntry->ui16ClusterHi << 16;
                    }
                    GetNextFileSector(ui32FirstCluster);
                    return(pDirEntry->ui32FileSize);
                }
                pDirEntry++;
            }
            if(sPartInfo.ui32Type == 16)
            {
                sPartInfo.ui16MaxRootEntries -= 512 / 32;
                if(sPartInfo.ui16MaxRootEntries)
                {
                    ui32DirSector++;
                }
                else
                {
                    return(0);
                }
            }
            else
            {
            }
       }
}
#define ReadFileSector() GetNextFileSector(0)

U32 ReadAppAndProgram(update object){
    updater* instance = (updater*)object;
        UF32 ui32FlashEnd;
        UF32 ui32FileSize;
        UF32 ui32DataSize;
        UF32 ui32Remaining;
        UF32 ui32ProgAddr;
        UF32 ui32SavedRegs[2];
        volatile UF32 ui32Idx;
        UF32 ui32DriveTimeout;

            ui32DriveTimeout = instance->Config->number_check   ;
            while(USBHMSCDriveReady(g_psMSCInstance))
            {
                SysCtlDelay(g_ui32SysClock/(3*2));
                ui32DriveTimeout--;
                if(ui32DriveTimeout == 0)
                {
                    return(1);
                }
            }
            if(Init_file(g_ui8SectorBuf))
                {
                    return(1);
                }
            ui32FileSize = Open_file(instance);
              if(ui32FileSize == 0)
              {
                  return(1);
              }
              ui32FlashEnd = FLASH_SIZE;
             #ifdef FLASH_RSVD_SPACE
                 ui32FlashEnd -= FLASH_RSVD_SPACE;
             #endif

             #ifndef FLASH_CODE_PROTECTION
                 ui32FlashEnd = ui32FileSize + (U32)(instance->Config->start_app_add);
             #endif
                 if((ui32FileSize + (U32)(instance->Config->start_app_add)) > ui32FlashEnd)
                    {
                        return(1);
                    }
                 for(ui32Idx = (U32)(instance->Config->start_app_add); ui32Idx < ui32FlashEnd; ui32Idx += 1024)
                    {
                        ROM_FlashErase(ui32Idx);
                    }
                    ui32ProgAddr = (U32)(instance->Config->start_app_add);
                    ui32Remaining = ui32FileSize;
                    while(ReadFileSector())
                    {
                        ui32DataSize = ui32Remaining >= 512 ? 512 : ui32Remaining;
                        ui32Remaining -= ui32DataSize;
                        if(ui32ProgAddr == (U32)(instance->Config->start_app_add))
                        {
                            U32 *pui32Temp;
                            pui32Temp = (U32 *)g_ui8SectorBuf;
                            ui32SavedRegs[0] = pui32Temp[0];
                            ui32SavedRegs[1] = pui32Temp[1];
                            ROM_FlashProgram((U32 *)&g_ui8SectorBuf[8],
                                             ui32ProgAddr + 8,
                                             ((ui32DataSize - 8) + 3) & ~3);
                        }
                        else{
                            ROM_FlashProgram((U32 *)g_ui8SectorBuf, ui32ProgAddr,
                                                         (ui32DataSize + 3) & ~3);
                        }
                        if(ui32Remaining)
                               {
                                   ui32ProgAddr += ui32DataSize;
                               }
                        {
                                   ROM_FlashProgram((U32 *)ui32SavedRegs, (U32)(instance->Config->start_app_add),
                                                     8);

                                   return(0);
                               }


                    }
                    return (1) ;
}
void config_usb()
{U32 ui32PLLRate;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
    SysCtlDelay(80);
    uDMAEnable();
    uDMAControlBaseSet(g_sDMAControlTable);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_USB0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
    GPIOPinConfigure(GPIO_PD6_USB0EPEN);
    GPIOPinTypeUSBDigital(GPIO_PORTD_BASE, GPIO_PIN_6);
    GPIOPinTypeUSBAnalog(GPIO_PORTL_BASE, GPIO_PIN_6 | GPIO_PIN_7);
    GPIOPinTypeUSBAnalog(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    USBHCDRegisterDrivers(0, g_ppHostClassDrivers, NUM_CLASS_DRIVERS);
    g_psMSCInstance = USBHMSCDriveOpen(0, CallbacksfromMSDevice);
    USBHCDPowerConfigInit(0, USBHCD_VBUS_AUTO_HIGH | USBHCD_VBUS_FILTER);
    SysCtlVCOGet(SYSCTL_XTAL_25MHZ, &ui32PLLRate);
    USBHCDFeatureSet(0, USBLIB_FEATURE_CPUCLK, &g_ui32SysClock);
    USBHCDFeatureSet(0, USBLIB_FEATURE_USBPLL, &ui32PLLRate);
    USBStackModeSet(0, eUSBModeForceHost, 0);
    SysCtlDelay(g_ui32SysClock/100);
    USBHCDInit(0, g_pHCDPool, HCD_MEMORY_SIZE);
}


