/*
 * Boot.c
 *
 *  Created on: 4 août 2020
 *      Author: WidedCherif
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "Boot.h"
#include "Updater.h"
#include "BswComponentManager.h"
#include "BasicTypes.h"

typedef struct Boot_struct {
      BswComponent Component;
      BootConfig* Config;

}Boot;



void CallApplication(bootloader object);
void Boot_main(bootloader objectb, update objectu);

bootloader Boot_createInstance(const Char* name, BootConfig* config) {
      Boot* instance = (Boot*)Mem_alloc(sizeof(Boot));
    if(instance != NULL)
        {        instance->Config = config;
        (void)BswComponentManager_onComponentCreated((BswComponent*)instance, config, name, "Boot", &Boot_init, &Boot_start, &Boot_stop, false, false);


       }
        return instance;
}
Bool Boot_init() {
    return 0;
}
Bool Boot_start () {

return 0;
}
Bool  Boot_stop () {
return 0 ;
}
void Boot_main(bootloader objectb, UPDATE objectu){

    U32 *pui32App;
    Boot* instance1 = (Boot*)objectb;
 //   update instance2 =objectu;
         pui32App =(instance1->Config->start_app_add);

    if(HWREG((U32)(instance1->Config->force_update_adr)) == ((U32)(instance1->Config->force_update_value)))
            { HWREG((U32)(instance1->Config->force_update_adr)) = 0;
               Updater_main(objectu);
            }
    if((pui32App[0] == 0xffffffff) ||
      ((pui32App[0] & 0xfff00000) != 0x20000000) ||
      (pui32App[1] == 0xffffffff) ||
      ((pui32App[1] & 0xfff00001) != 0x00000001))

    { Updater_main(objectu);
  }
    CallApplication(instance1);
}
void AppForceUpdate(bootloader objectb)
{Boot* instance = (Boot*)objectb;
    HWREG((U32)(instance->Config->force_update_adr)) = (U32)(instance->Config->force_update_value);
    HWREG(NVIC_APINT) = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
}

void CallApplication(bootloader objectb)
{Boot* instance = (Boot*)objectb;
    HWREG(NVIC_VTABLE) = (U32)(instance->Config->start_app_add);

    __asm("    ldr     r1, [r0]\n"
          "    mov     sp, r1\n");

    __asm("    ldr     r0, [r0, #4]\n"
          "    bx      r0\n");
}
