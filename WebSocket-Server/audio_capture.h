#pragma once
#include <functional>
#include <vector>

class AudioCapture {
public:
    using Callback = std::function<void(const uint8_t*, size_t)>;

    bool start(Callback cb);
    void stop();

private:
    Callback callback;
    bool running = false;
};
