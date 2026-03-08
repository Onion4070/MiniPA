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

        WAVEFORMATEXTENSIBLE* wfext = nullptr;
        if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            wfext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format);
            wfext->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        }
        format->wBitsPerSample = 32;
        format->nSamplesPerSec = 48000;
        format->nChannels = 2;
        format->nBlockAlign = format->nChannels * format->wBitsPerSample / 8;
        format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
        if (wfext) wfext->Samples.wValidBitsPerSample = 32;

        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | 
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            100000, // 10ms 
            0, 
            format, 
            nullptr);
        client->SetEventHandle(hEvent);

        IAudioCaptureClient* capture;
        client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);

        client->Start();

        while (running) {
            WaitForSingleObject(hEvent, INFINITE);
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
        }
        CloseHandle(hEvent);
    }).detach();

    return true;
}

void AudioCapture::stop() {
    running = false;
}
