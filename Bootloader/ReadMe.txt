Documentation
This Bootloader was developped using the texas instruments board TM4C129
What is a Bootloader:
 * The Bootloader is a code launched when we initialize a hardware or when a need arises in order to load upgrades/updates/Main software to the hardware.
Boot module:
   ** Check if wa need an Update / Upgrade: in the case of a Hardware start-up or a forcing update.
   ** start the software programmed in the flash memory.
Updater module:
  ** USB configuration
  ** Verification of the presence of the USB
  ** Verification of the file system and the file specifications
  ** program the application in flash memory of the board
