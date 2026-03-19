#include "volume_macos.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#define VOLUME_STEP 0.05f

static AudioDeviceID g_device_id = kAudioDeviceUnknown;

static AudioDeviceID get_default_output_device(void)
{
    AudioDeviceID device_id = kAudioDeviceUnknown;
    UInt32 size = sizeof(device_id);

    AudioObjectPropertyAddress prop = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &prop, 0, NULL, &size, &device_id);

    if (status != noErr) {
        return kAudioDeviceUnknown;
    }
    return device_id;
}

int rc_volume_init(void)
{
    g_device_id = get_default_output_device();
    if (g_device_id == kAudioDeviceUnknown) {
        fprintf(stderr, "[volume] Failed to get default audio device\n");
        return -1;
    }

    fprintf(stdout, "[volume] macOS CoreAudio initialized (device %u)\n", g_device_id);
    return 0;
}

void rc_volume_shutdown(void)
{
    g_device_id = kAudioDeviceUnknown;
}

static float get_volume(void)
{
    Float32 volume = 0.0f;
    UInt32 size = sizeof(volume);

    AudioObjectPropertyAddress prop = {
        .mSelector = kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioObjectGetPropertyData(g_device_id, &prop, 0, NULL, &size, &volume);
    return volume;
}

static void set_volume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    Float32 vol = volume;

    AudioObjectPropertyAddress prop = {
        .mSelector = kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioObjectSetPropertyData(g_device_id, &prop, 0, NULL, sizeof(vol), &vol);
}

static UInt32 get_mute(void)
{
    UInt32 muted = 0;
    UInt32 size = sizeof(muted);

    AudioObjectPropertyAddress prop = {
        .mSelector = kAudioDevicePropertyMute,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioObjectGetPropertyData(g_device_id, &prop, 0, NULL, &size, &muted);
    return muted;
}

static void set_mute(UInt32 muted)
{
    AudioObjectPropertyAddress prop = {
        .mSelector = kAudioDevicePropertyMute,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioObjectSetPropertyData(g_device_id, &prop, 0, NULL, sizeof(muted), &muted);
}

void rc_volume_handle(uint8_t action)
{
    if (g_device_id == kAudioDeviceUnknown) return;

    switch (action) {
    case RC_VOL_UP:
        set_volume(get_volume() + VOLUME_STEP);
        break;
    case RC_VOL_DOWN:
        set_volume(get_volume() - VOLUME_STEP);
        break;
    case RC_VOL_MUTE:
        set_mute(get_mute() ? 0 : 1);
        break;
    }
}
