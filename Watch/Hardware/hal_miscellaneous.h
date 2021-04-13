//==============================================================================
//  Copyright 2011 Meta Watch Ltd. - http://www.MetaWatch.org/
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

/******************************************************************************/
/*! \file hal_miscellaneous.h
*
* Set pins to outputs; setup clock and power management module
*/
/******************************************************************************/

#ifndef HAL_MISCELLANEOUS_H
#define HAL_MISCELLANEOUS_H

#define BT_LOCAL_NAME   "MetaWatch   "
#define LOCAL_NAME_LENGTH   (sizeof(BT_LOCAL_NAME)) //max 12 chars

/*! Reads the hardware revision register in the MSP430
 *
 * \return Character representation of revision (Example: 'E')
 */
unsigned char GetMsp430HardwareRevision(void);

/*! Revision F and H solve these errata
 *
 * ADC27, FLASH37, PMM10, PMM11, PMM12, PMM17, SYS10, SYS12, 
 * TAB23, UCS6, UCS9, UCS10, USCII30
 *
 * Revision G is actually revision E silicon from a different fab house so
 * the check is not as simple as it could be.
 *
 * This be called ErrataGroup1.
 *
 * These could also be compiled out at some time (or even removed from source)
 */
/*! return 1 if workarounds for errata group 1 are required */
unsigned char Errata(void);

/*! Setup the CPU clock and the power managment module.
 * This function uses TI provided routines to start the crystal
 * and setup the FLL.
 *
 * the core voltage must be set appropriately for the MHz that is desired
 *
 */
void SetupClockAndPowerManagementModule(void);


/*! setup pins that aren't setup by specific functions 
 * catch-all
 * \note Board Configuration must be valid before this is called
 */
void ConfigureDefaultIO(void);

/******************************************************************************/

/*! The board revision was going to be used to determine what patch to load
 *
 */
unsigned char GetBoardConfiguration(void);

#endif /* HAL_MISCELLANEOUS_H */
