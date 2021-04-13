//==============================================================================
//  Copyright 2013 Meta Watch Ltd. - http://www.MetaWatch.org/
// 
//  Licensed under the Meta Watch License, Version 1.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//  
//      http://www.MetaWatch.org/licenses/license-1.0.html
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//==============================================================================

#include "FreeRTOS.h"
#include "hal_lpm.h"
#include "hal_boot.h"
#include "hal_battery.h"
#include "DebugUart.h"
#include "MuxMode.h"
#include "hal_board_type.h"
#include "Log.h"

#if __IAR_SYSTEMS_ICC__
__no_init __root unsigned char niMuxMode @ MUX_MODE_ADDR;
#else
extern unsigned char niMuxMode;
#endif

/* Assume clip is on */
unsigned char GetMuxMode(void)
{
  return niMuxMode;
}

// from service menu
void SetMuxMode(unsigned char MuxMode)
{
  niMuxMode = MuxMode;
  ChangeMuxMode(ClipOn());
}

/******************************************************************************/
/* This only applies to board configuration 5 and later
 * Since these pins were previously unused the board configuration is not checked
 * This function does not modify the value stored in flash that is used 
 * during start-up
 * The RST/NMI pin is handled separately
 * This is used by the bootloader so don't use DebugUart.
 */
void ChangeMuxMode(unsigned char Clip)
{
  static unsigned char LastMode = MUX_MODE_OFF;

//  if (niResetType != NORMAL_RESET) niMuxMode = MUX_MODE_SERIAL;

  niMuxMode = Clip ? MUX_MODE_SERIAL : MUX_MODE_GND;
  if (niMuxMode == LastMode) return;

  /* setup default state for mux */
  ENABLE_MUX_OUTPUT_CONTROL();

  switch (niMuxMode)
  {
  case MUX_MODE_SERIAL: // with bootloader for flashing
    MUX_OUTPUT_SELECTS_SERIAL();
    EnableDebugUart(Clip);
    break;
  
  case MUX_MODE_SPY_BI_WIRE: // TI jtag in-circuit debugging
    MUX_OUTPUT_SELECTS_SPY();
    break;
    
  case MUX_MODE_GND: // normal mode
    /* this mode prevents shorting of the case back pins due to sweat,
     * screwdrivers, etc */
  default:
    MUX_OUTPUT_SELECTS_GND();
    break;
  }

  LastMode = niMuxMode;
}
