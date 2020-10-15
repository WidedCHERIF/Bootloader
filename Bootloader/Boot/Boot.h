/*
 * Boot.h
 *
 *  Created on: 4 août 2020
 *      Author: WidedCherif
 */

#ifndef BOOT_H_
#define BOOT_H_

#include "BasicTypes.h"

typedef void* bootloader;
typedef void* UPDATE;

typedef struct BootConfig_struct
{
U32* start_app_add;
U32* force_update_adr;
U32* force_update_value;
}BootConfig;

bootloader Boot_createInstance(const Char* name, BootConfig* config);
Bool Boot_init ();
Bool Boot_start();
Bool Boot_stop() ;
extern void Boot_main(bootloader objectb, UPDATE objectu) ;

static void CallApplication(bootloader objectb);
void AppForceUpdate(bootloader objectb);






#endif /* BOOT_H_ */
