/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PULSES_H_
#define _PULSES_H_

enum ModuleSettingsMode
{
  MODULE_MODE_NORMAL,
  MODULE_MODE_RANGECHECK,
  MODULE_MODE_BIND
};

//extern ModuleState moduleState[NUM_MODULES];

#if NUM_MODULES > 1
  #define IS_RANGECHECK_ENABLE()             (moduleState[0].mode == MODULE_MODE_RANGECHECK || moduleState[1].mode == MODULE_MODE_RANGECHECK)
#else
  #define IS_RANGECHECK_ENABLE()             (moduleState[0].mode == MODULE_MODE_RANGECHECK)
#endif

#if defined(DSM2) && !defined(PCBTARANIS)
  #define DSM2_BIND_TIMEOUT      255         // 255*11ms
  extern uint8_t dsm2BindTimer;
#endif

  #define IS_PPM_PROTOCOL(protocol)          (protocol==PROTOCOL_CHANNELS_PPM)

#if defined(PXX)
  #define IS_PXX_PROTOCOL(protocol)          (protocol==PROTO_PXX)
#else
  #define IS_PXX_PROTOCOL(protocol)          (0)
#endif

#if defined(DSM2)
  #define IS_DSM2_PROTOCOL(protocol)         (protocol>=PROTO_DSM2_LP45 && protocol<=PROTO_DSM2_DSMX)
#else
  #define IS_DSM2_PROTOCOL(protocol)         (0)
#endif

#if defined(DSM2_SERIAL)
  #define IS_DSM2_SERIAL_PROTOCOL(protocol)  (IS_DSM2_PROTOCOL(protocol))
#else
  #define IS_DSM2_SERIAL_PROTOCOL(protocol)  (0)
#endif

#if defined(MULTIMODULE)
  #define IS_MULTIMODULE_PROTOCOL(protocol)  (protocol==PROTOCOL_CHANNELS_MULTIMODULE)
  #if !defined(DSM2)
     #error You need to enable DSM2 = PPM for MULTIMODULE support
  #endif
#else
  #define IS_MULTIMODULE_PROTOCOL(protocol)  (0)
#endif

  #define IS_SBUS_PROTOCOL(protocol)         (protocol == PROTOCOL_CHANNELS_SBUS)

  #include "pulses_arm.h"
void intmoduleStop();
void intmoduleStop();
void extmoduleStop();

void intmoduleNoneStart();
void intmoduleAfhds2aStart();

void extmodulePpmStart();
void extmoduleTimerStart(uint32_t period, uint8_t state);

#endif // _PULSES_H_
