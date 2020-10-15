/*
 * Updater.h
 *
 *  Created on: 4 août 2020
 *      Author: WidedCherif
 */

#ifndef UPDATER_H_
#define UPDATER_H_

#include "BasicTypes.h"
#include "BswComponentManager.h"
typedef void* update;

typedef struct UpdaterConfig_struct
{
    U8 number_check;
    U32* start_app_add;
    U8 namesize;
    Char  FileName[11];
}UpdaterConfig;



update Updater_createInstance(const Char* name, UpdaterConfig* config);
Bool Updater_init ();
void Updater_start(update object);
Bool Updater_stop() ;
extern void Updater_main(update object) ;


static void config_usb() ;
static U32 Init_file(U8 *pui8SectorBuf);
static U32 GetNextCluster(UF32 ui32ThisCluster);
static U32 ReadMediaSector(UF32 ui32Sector, U8 *pui8Buf);
static U32 GetNextFileSector(UF32 ui32StartCluster);
static U32 Open_file(update object);
static U32 ReadAppAndProgram(update object);




#endif /* UPDATER_H_ */
