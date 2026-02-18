#include "audio_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>

bool AudioCapture::start(Callback cb) {
    callback = cb;
    running = true;

    std::thread([this] {

        CoInitialize(nullptr);

        IMMDeviceEnumerator* enumerator = nullptr;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator);

        IMMDevice* device = nullptr;
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

        IAudioClient* client = nullptr;
        device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);

        WAVEFORMATEX* format;
        client->GetMixFormat(&format);

        client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            0, 0, format, nullptr);

        IAudioCaptureClient* capture;
        client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);

        client->Start();

        while (running) {
            UINT32 packet = 0;
            capture->GetNextPacketSize(&packet);

            while (packet) {
                BYTE* data;
                UINT32 frames;
                DWORD flags;

                capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);

                if (callback)
                    callback(data, frames * format->nBlockAlign);

                capture->ReleaseBuffer(frames);
                capture->GetNextPacketSize(&packet);
            }
            Sleep(1);
        }

        }).detach();

    return true;
}

void AudioCapture::stop() {
    running = false;
}
