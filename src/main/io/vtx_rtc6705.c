/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Created by jflyper */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#if defined(USE_VTX_RTC6705) && defined(USE_VTX_CONTROL)

#include "build/build_config.h"
#include "build/debug.h"

#include "cms/cms.h"
#include "cms/cms_types.h"

#include "common/maths.h"
#include "common/utils.h"

#include "config/feature.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "drivers/max7456.h"
#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/vtx_rtc6705.h"
#include "drivers/vtx_common.h"

#include "fc/config.h"
#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "io/vtx.h"
#include "io/vtx_rtc6705.h"
#include "io/vtx_string.h"

#if defined(USE_CMS) || defined(USE_VTX_COMMON)
const char * const rtc6705PowerNames[] = {
    "OFF", "MIN", "MAX"
};
#endif

#ifdef USE_VTX_COMMON
static vtxVTable_t rtc6705VTable;    // Forward
static vtxDevice_t vtxRTC6705 = {
    .vTable = &rtc6705VTable,
    .capability.bandCount = VTX_SETTINGS_BAND_COUNT,
    .capability.channelCount = VTX_SETTINGS_CHANNEL_COUNT,
    .capability.powerCount = VTX_RTC6705_POWER_COUNT,
    .bandNames = (char **)vtx58BandNames,
    .channelNames = (char **)vtx58ChannelNames,
    .powerNames = (char **)rtc6705PowerNames,
};
#endif

bool vtxRTC6705Init(void)
{
    vtxCommonRegisterDevice(&vtxRTC6705);

    return true;
}

void vtxRTC6705Configure(void)
{
    rtc6705SetRFPower(vtxRTC6705.powerIndex);
    rtc6705SetBandAndChannel(vtxRTC6705.band - 1, vtxRTC6705.channel - 1);
}

bool vtxRTC6705CanUpdate(void)
{
#if defined(MAX7456_SPI_INSTANCE) && defined(RTC6705_SPI_INSTANCE) && defined(SPI_SHARED_MAX7456_AND_RTC6705)
    if (feature(FEATURE_OSD)) {
        return !max7456DmaInProgress();
    }
#endif
    return true;
}

#ifdef RTC6705_POWER_PIN
static void vtxRTC6705EnableAndConfigure(void)
{
    while (!vtxRTC6705CanUpdate());

    rtc6705Enable();

    delay(VTX_RTC6705_BOOT_DELAY);

    vtxRTC6705Configure();
}
#endif

void vtxRTC6705Process(timeUs_t now)
{
    UNUSED(now);
}

#ifdef USE_VTX_COMMON
// Interface to common VTX API

vtxDevType_e vtxRTC6705GetDeviceType(void)
{
    return VTXDEV_RTC6705;
}

bool vtxRTC6705IsReady(void)
{
    return true;
}

void vtxRTC6705SetBandAndChannel(uint8_t band, uint8_t channel)
{
    while (!vtxRTC6705CanUpdate());

    if (band && channel) {
        if (vtxRTC6705.powerIndex > 0) {
            rtc6705SetBandAndChannel(band - 1, channel - 1);

            vtxRTC6705.band = band;
            vtxRTC6705.channel = channel;
        }
    }
}

void vtxRTC6705SetPowerByIndex(uint8_t index)
{
    while (!vtxRTC6705CanUpdate());

#ifdef RTC6705_POWER_PIN
    if (index == 0) {
        // power device off
        if (vtxRTC6705.powerIndex > 0) {
            // on, power it off
            vtxRTC6705.powerIndex = index;
            rtc6705Disable();
            return;
        } else {
            // already off
        }
    } else {
        // change rf power and maybe turn the device on first
        if (vtxRTC6705.powerIndex == 0) {
            // if it's powered down, power it up, wait and configure channel, band and power.
            vtxRTC6705.powerIndex = index;
            vtxRTC6705EnableAndConfigure();
            return;
        } else {
            // if it's powered up, just set the rf power
            vtxRTC6705.powerIndex = index;
            rtc6705SetRFPower(index);
        }
    }
#else
    vtxRTC6705.powerIndex = MAX(index, VTX_RTC6705_MIN_POWER);
    rtc6705SetRFPower(index);
#endif
}

void vtxRTC6705SetPitMode(uint8_t onoff)
{
    UNUSED(onoff);
    return;
}

void vtxRTC6705SetFreq(uint16_t freq)
{
    UNUSED(freq);
    return;
}

bool vtxRTC6705GetBandAndChannel(uint8_t *pBand, uint8_t *pChannel)
{
    *pBand = vtxRTC6705.band;
    *pChannel = vtxRTC6705.channel;
    return true;
}

bool vtxRTC6705GetPowerIndex(uint8_t *pIndex)
{
    *pIndex = vtxRTC6705.powerIndex;
    return true;
}

bool vtxRTC6705GetPitMode(uint8_t *pOnOff)
{
    UNUSED(pOnOff);
    return false;
}

bool vtxRTC6705GetFreq(uint16_t *pFreq)
{
    UNUSED(pFreq);
    return false;
}

static vtxVTable_t rtc6705VTable = {
    .process = vtxRTC6705Process,
    .getDeviceType = vtxRTC6705GetDeviceType,
    .isReady = vtxRTC6705IsReady,
    .setBandAndChannel = vtxRTC6705SetBandAndChannel,
    .setPowerByIndex = vtxRTC6705SetPowerByIndex,
    .setPitMode = vtxRTC6705SetPitMode,
    .setFrequency = vtxRTC6705SetFreq,
    .getBandAndChannel = vtxRTC6705GetBandAndChannel,
    .getPowerIndex = vtxRTC6705GetPowerIndex,
    .getPitMode = vtxRTC6705GetPitMode,
    .getFrequency = vtxRTC6705GetFreq,
};
#endif // VTX_COMMON

#endif // VTX_RTC6705
