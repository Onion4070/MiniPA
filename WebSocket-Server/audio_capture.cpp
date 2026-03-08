#include "audio_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>

bool AudioCapture::start(Callback cb) {
    callback = cb;
    running = true;

    std::thread([this] {

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) return; // 失敗したら即終了

        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if (FAILED(hr) || !enumerator) return;

        IMMDevice* device = nullptr;
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (!device) return;

        IAudioClient* client = nullptr;
        device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
        if (!client) return;

        WAVEFORMATEX* format;
        client->GetMixFormat(&format);
        if (!format) return;

        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (hEvent == NULL) return; // NULLチェック追加

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
        if (!capture) return;

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
