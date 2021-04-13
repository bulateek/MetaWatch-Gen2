//==============================================================================
//  Copyright 2011-2013 Meta Watch Ltd. - http://www.MetaWatch.org/
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
#include "queue.h"
#include "task.h"
#include "Messages.h"
#include "hal_board_type.h"
#include "hal_boot.h"
#include "hal_rtc.h"
#include "hal_lpm.h"
#include "hal_calibration.h"
#include "DebugUart.h"
#include "Statistics.h"
#include "OneSecondTimers.h"
#include "Wrapper.h"
#include "Vibration.h"
#include "LcdDisplay.h"
#include "Log.h"

/** Real Time Clock interrupt Flag definitions */
#define RTC_NO_INTERRUPT        0
#define RTC_RDY_IFG             2
#define RTC_EV_IFG              4
#define RTC_A_IFG               6
#define RTC_PRESCALE_ZERO_IFG   8
#define RTC_PRESCALE_ONE_IFG    10

#define RTCCAL_VALUE_MASK     (0x3F)
#define RTC_USER_MASK     (RTC_TIMER_VIBRATION | RTC_TIMER_BUTTON)

#if __IAR_SYSTEMS_ICC__
__no_init __root unsigned char niRtcMin @RTC_MIN_ADDR;
__no_init __root unsigned char niRtcHour @RTC_HOUR_ADDR;
__no_init __root unsigned char niRtcDay @RTC_DAY_ADDR;
__no_init __root unsigned char niRtcMon @RTC_MON_ADDR;
__no_init __root unsigned char niRtcDow @RTC_DOW_ADDR;
__no_init __root unsigned int niRtcYear @RTC_YEAR_ADDR;
#else
extern unsigned char niRtcMin;
extern unsigned char niRtcHour;
extern unsigned char niRtcDay;
extern unsigned char niRtcMon;
extern unsigned char niRtcDow;
extern unsigned int niRtcYear;
#endif

static const unsigned char MaxRtc[] = {59, 59, 23, 31, 6, 12};
static unsigned char RtcInUseMask = 0;

static void RestoreRtc(void);
static unsigned char IncBCD(unsigned char Rtc, unsigned char Index);

void InitRealTimeClock(void)
{
  RtcInUseMask = 0;
  
  // stop it
  RTCCTL01 |= RTCHOLD;

  // use calibration data to adjust real time clock frequency
  if (ValidCalibration())
  {
    signed char RtcCalibrationValue = GetRtcCalibrationValue();
    
    if (RtcCalibrationValue < 0)
    {
      RtcCalibrationValue = -RtcCalibrationValue;
      RTCCTL2 = RtcCalibrationValue & RTCCAL_VALUE_MASK;
    }
    else
    {
      RTCCTL2 = RtcCalibrationValue & RTCCAL_VALUE_MASK;
      /* adjust up */
      RTCCTL2 |= RTCCALS;
    }
  }
  
  // Set RTC mode and BCD format
  RTCCTL01 |= RTCMODE | RTCBCD;

  // set 128 Hz rate for prescale 0 interrupt
  RTCPS0CTL |= RT0IP_7;

  // enable 1 pulse per second interrupt using prescale 1
  RTCPS1CTL |= RT1IP_6 | RT1PSIE;

  // 1 Hz calibration output
  RTCCTL23 |= RTCCALF_3;

  // setting the peripheral selection bit makes the other I/O control a don't care
  // P2.4 = 1 Hz RTC calibration output
  // Direction needs to be set as output
  RTC_1HZ_PORT_SEL |= RTC_1HZ_BIT;  
  RTC_1HZ_PORT_DIR |= RTC_1HZ_BIT;

  RestoreRtc();

  // Enable the RTC
  RTCCTL01 &= ~RTCHOLD;  
}

unsigned char SetRtc(Rtc_t *pRtcData)
{
  static unsigned char Once;
  unsigned char Hour = ToBCD(pRtcData->Hour);
  unsigned char Minute = ToBCD(pRtcData->Minute);

  if (Once && Hour == niRtcHour && Minute == niRtcMin) return FALSE;

  Once = TRUE;
  niRtcMin = Minute;
  niRtcHour = Hour;
  niRtcDay = ToBCD(pRtcData->Day);
  niRtcDow = ToBCD(pRtcData->DayOfWeek);
  niRtcMon = ToBCD(pRtcData->Month);

  // These calls are to asm level patch functions provided by TI for the MSP430F5438
  unsigned int Year = (pRtcData->YearMsb << 8) + pRtcData->YearLsb;
  PrintF(">SetRtc: %d", Year);
  niRtcYear = (ToBCD(Year / 100) << 8) + ToBCD(Year % 100);

  // Stop the RTC
  RTCCTL01 |= RTCHOLD;    

  RTCYEAR = niRtcYear;
  RTCMON = niRtcMon;
  RTCDAY = niRtcDay;
  RTCDOW = niRtcDow;
  RTCHOUR = niRtcHour;
  RTCMIN = niRtcMin;
  RTCSEC = ToBCD(pRtcData->Second);

  // Enable the RTC
  RTCCTL01 &= ~RTCHOLD;
  return TRUE;
}

static void RestoreRtc(void)
{
  if (niResetType == NORMAL_RESET &&
      BCD_H(niRtcHour) >= 0 && BCD_H(niRtcHour) <= 2 &&
      BCD_L(niRtcHour) >= 0 && BCD_L(niRtcHour) <= 9 &&
      BCD_H(niRtcMin) >= 0 && BCD_H(niRtcMin) <= 6 &&
      BCD_L(niRtcMin) >= 0 && BCD_L(niRtcMin) <= 9)
  {
    RTCYEAR = niRtcYear;
    RTCMON = niRtcMon;
    RTCDAY = niRtcDay;
    RTCDOW = niRtcDow;
    RTCHOUR = niRtcHour;
    RTCMIN = niRtcMin;
  }
  else
  {
    RTCYEARH = 0x20;
    RTCYEARL = 0x13;
    RTCMON = 0x05;
    RTCDAY = 0x23;
    RTCDOW = 0x05;
    RTCHOUR = 0x11;
    RTCMIN = 0x58;
    RTCSEC = 0x0;    
  }
}

unsigned char ToBCD(unsigned char Bin)
{
  unsigned char Quotient = 0;
  while (Bin >= 10)
  {
    Bin -= 10;
    Quotient ++;
  }
  return (Quotient << 4) + Bin;
}

unsigned char ToBin(unsigned char Bcd)
{
 return (((BCD_H(Bcd) << 2) + BCD_H(Bcd)) << 1) + BCD_L(Bcd);
}

void IncRtc(unsigned char Index)
{
  switch (Index)
  {
  case RTC_DAY: RTCDAY = IncBCD(RTCDAY, Index); break;
  case RTC_DOW: RTCDOW = IncBCD(RTCDOW, Index); break;
  case RTC_HOUR: RTCHOUR = IncBCD(RTCHOUR, Index); break;
  case RTC_MIN: RTCMIN = IncBCD(RTCMIN, Index); break;
  default: break;
  }
}

static unsigned char IncBCD(unsigned char Rtc, unsigned char Index)
{
  Rtc = ToBin(Rtc) + 1;
  
  if (Rtc > MaxRtc[Index]) Rtc = (Index == RTC_DAY || Index == RTC_MON) ? 1 : 0;

  return ToBCD(Rtc);
}

unsigned char To12H(unsigned char H24)
{
  unsigned char HH = BCD_H(H24);
  unsigned char HL = BCD_L(H24);

  if (H24 == 0) {HH = 1; HL = 2;} // 0am -> 12am
  else if (HH == 1 && HL > 2) {HH = 0; HL -= 2;}
  else if (HH == 2) {HH = 0; HL += 8; if (HL > 9) {HL -= 10; HH ++;}}
  return (HH << 4) + HL;
}

void BackupRtc(void)
{
  niRtcYear = RTCYEAR;
  niRtcMon = RTCMON;
  niRtcDay = RTCDAY;
  niRtcDow = RTCDOW;
  niRtcHour = RTCHOUR;
  niRtcMin = RTCMIN;
}

void EnableRtcPrescaleInterruptUser(unsigned char UserMask)
{
  portENTER_CRITICAL();
   
  // If not currently enabled, enable the timer ISR
//  if((RtcInUseMask & RTC_USER_MASK) == 0)
//  {
    RTCPS0CTL |= RT0PSIE;
//  }

  RtcInUseMask |= UserMask;
   
  portEXIT_CRITICAL();
}

void DisableRtcPrescaleInterruptUser(unsigned char UserMask)
{

  portENTER_CRITICAL();
  
  RtcInUseMask &= ~UserMask;
      
  // If there are no more users, disable the interrupt
  if ((RtcInUseMask & RTC_USER_MASK) == 0)
  {
    RTCPS0CTL &= ~RT0PSIE;
  }
  
  portEXIT_CRITICAL();
}

static unsigned char DivideByFour = 0;

/*! Real Time Clock interrupt handler function.
 *
 *  Used for system timing.  When the processor is in low power mode, the RTC
 *  is the only timer running so we use it as a system timer.  There are two
 *  different timers.  RTC prescale one occurs at a 32 Hz rate and is enabled
 *  as needed.  The RTC Ready event occurs at 1 ppS and is always enabled.
 *
 * don't exit LPM3 unless it is required
 */
#pragma vector = RTC_VECTOR
__interrupt void RTC_ISR(void)
{
  unsigned char ExitLpm = 0;

  // compiler intrinsic, value must be even, and in the range of 0 to 10
  switch(__even_in_range(RTCIV,10))
  {
  case RTC_NO_INTERRUPT: break;
  case RTC_RDY_IFG:      break;
  case RTC_EV_IFG:       break;
  case RTC_A_IFG:        break;

  case RTC_PRESCALE_ZERO_IFG:

    // divide by four to get 32 Hz
    if (DivideByFour >= 4-1)
    {
      DivideByFour = 0;
           
      if (RtcInUseMask & RTC_TIMER_VIBRATION) VibrationMotorStateMachineIsr();
      
      if (RtcInUseMask & RTC_TIMER_BUTTON)
      {
        SendMessageIsr(ButtonStateMsg, MSG_OPT_NONE);
        ExitLpm = 1;
      }
    }
    else
    {
      DivideByFour ++;
    }
    break;

  case RTC_PRESCALE_ONE_IFG:
    
#ifdef DIGITAL
    ExitLpm |= LcdRtcUpdateHandlerIsr();
#endif
    
    ExitLpm |= OneSecondTimerHandlerIsr();
    break;
  
  default:
    break;
  }

  if (ExitLpm) EXIT_LPM_ISR();
}
