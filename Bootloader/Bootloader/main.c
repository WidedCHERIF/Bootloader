

/**
 * main.c
 */
//include libraries
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "Updater_USB.h"
#include "Boot.h"


#include "driverlib/sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/rom.h"



#define SIZE 11
#define CHECK 4
#define START_APPLICATION_ADRESS 0x8000
#define FORCE_UPDATE_ADDR 0x20004000
#define FORCE_UPDATE_VALUE 0x1234cdef
uint32_t g_ui32SysClock;
const char nameu="UPDATER";
const char nameb="BOOT";
const struct UpdaterConfig_struct Configu= {
                                                   .number_check=4,
                                                   .start_app_add=(uint32_t *)START_APPLICATION_ADRESS,
                                                   .namesize=SIZE,
                                                   .FileName={'F','I','R','M','W','A','R','E','B','I','N'}
};
const struct BootConfig_struct Configb= {
                                         .force_update_adr=(uint32_t *)FORCE_UPDATE_ADDR,
                                         .force_update_value=(uint32_t *)FORCE_UPDATE_VALUE,
                                         .start_app_add=(uint32_t *)START_APPLICATION_ADRESS
};


int main(void)
{
   g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                           SYSCTL_OSC_MAIN |
                                           SYSCTL_USE_PLL |
                                           SYSCTL_CFG_VCO_480), 120000000);
   ConfigureUART();
   FPULazyStackingEnable();
   updater objectu;
   Boot objectb;
   Updater_createInstance(&nameu, &Configu);
   Boot_createInstance(&nameb, &Configb);
   objectu.Config=&Configu;
   objectb.Config=&Configb;

       Boot_main(&objectb,&objectu);
//


	return 0;
}
