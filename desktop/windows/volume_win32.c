#include "volume_win32.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <initguid.h>

/* COM GUIDs */
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C,
            0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35,
            0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5CDF2C82, 0x841E, 0x4546,
            0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A);

static IAudioEndpointVolume *g_volume = NULL;

#define VOLUME_STEP 0.05f  /* 5% per step */

int rc_volume_init(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        fprintf(stderr, "[volume] COM init failed\n");
        return -1;
    }

    IMMDeviceEnumerator *enumerator = NULL;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&enumerator);
    if (FAILED(hr)) {
        fprintf(stderr, "[volume] Failed to create device enumerator\n");
        return -1;
    }

    IMMDevice *device = NULL;
    hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender,
                                                      eConsole, &device);
    enumerator->lpVtbl->Release(enumerator);

    if (FAILED(hr)) {
        fprintf(stderr, "[volume] Failed to get default audio endpoint\n");
        return -1;
    }

    hr = device->lpVtbl->Activate(device, &IID_IAudioEndpointVolume,
                                   CLSCTX_ALL, NULL, (void **)&g_volume);
    device->lpVtbl->Release(device);

    if (FAILED(hr)) {
        fprintf(stderr, "[volume] Failed to activate volume endpoint\n");
        return -1;
    }

    fprintf(stdout, "[volume] Windows Audio initialized\n");
    return 0;
}

void rc_volume_shutdown(void)
{
    if (g_volume != NULL) {
        g_volume->lpVtbl->Release(g_volume);
        g_volume = NULL;
    }
    CoUninitialize();
}

void rc_volume_handle(uint8_t action)
{
    if (g_volume == NULL) return;

    float current = 0.0f;
    BOOL is_muted = FALSE;

    switch (action) {
    case RC_VOL_UP:
        g_volume->lpVtbl->GetMasterVolumeLevelScalar(g_volume, &current);
        current += VOLUME_STEP;
        if (current > 1.0f) current = 1.0f;
        g_volume->lpVtbl->SetMasterVolumeLevelScalar(g_volume, current, NULL);
        break;

    case RC_VOL_DOWN:
        g_volume->lpVtbl->GetMasterVolumeLevelScalar(g_volume, &current);
        current -= VOLUME_STEP;
        if (current < 0.0f) current = 0.0f;
        g_volume->lpVtbl->SetMasterVolumeLevelScalar(g_volume, current, NULL);
        break;

    case RC_VOL_MUTE:
        g_volume->lpVtbl->GetMute(g_volume, &is_muted);
        g_volume->lpVtbl->SetMute(g_volume, !is_muted, NULL);
        break;
    }
}
