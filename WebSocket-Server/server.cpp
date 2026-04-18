#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <Windows.h>
#include <opus/opus.h>

#include "audio_capture.h"
#include "net_utils.h"
#include "session.h"
#include "terminal_qr.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

std::string load_file(const std::string& path) {
    std::ifstream t(path);
    return { std::istreambuf_iterator<char>(t),{} };
}

// ── HTTP接続を1件処理する（スレッドで呼ぶ） ──────────────────
void handle_connection(tcp::socket socket) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        if (websocket::is_upgrade(req) && req.target() == "/ws") {
            // WebSocketセッション開始
            auto s = std::make_shared<Session>(std::move(socket));
            s->run(std::move(req));   // run()は内部でスレッドを立てて待機
        }
        else {
            // 通常のHTTPレスポンス
            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, "Beast");
            res.set(http::field::content_type, "text/html; charset=utf-8");
            // キャッシュ無効化 (index.htmlの変更が即反映されるように)
            res.set(http::field::cache_control, "no-store");
            res.body() = load_file("index.html");
            res.prepare_payload();
            http::write(socket, res);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "handle_connection error: " << e.what() << std::endl;
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    std::string localIP = NetUtils::GetLocalIP();
    std::string url = "http://" + localIP + ":9001";
    TerminalQR::show(url.c_str());

    // ── Opusエンコーダ初期化 ─────────────────────────────────
    const int SAMPLE_RATE = 48000;
    const int CHANNELS = 2;
    const int FRAME_SIZE = 480; // 10ms @ 48kHz

    int opusErr = 0;
    OpusEncoder* encoder = opus_encoder_create(
        SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &opusErr);
    if (opusErr != OPUS_OK || !encoder) {
        std::cerr << "opus_encoder_create failed: " << opusErr << std::endl;
        return 1;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(128000));

    std::vector<float> pcm_accum;
    std::mutex         pcm_mutex;

    // ── 音声キャプチャ開始 ───────────────────────────────────
    AudioCapture cap;
    cap.start([&](const uint8_t* data, size_t size) {
        const float* f = reinterpret_cast<const float*>(data);
        size_t       nSamples = size / sizeof(float);
        {
            std::lock_guard<std::mutex> lock(pcm_mutex);
            pcm_accum.insert(pcm_accum.end(), f, f + nSamples);
        }

        while (true) {
            std::vector<float> frame;
            {
                std::lock_guard<std::mutex> lock(pcm_mutex);
                if ((int)pcm_accum.size() < FRAME_SIZE * CHANNELS) break;
                frame.assign(pcm_accum.begin(),
                    pcm_accum.begin() + FRAME_SIZE * CHANNELS);
                pcm_accum.erase(pcm_accum.begin(),
                    pcm_accum.begin() + FRAME_SIZE * CHANNELS);
            }

            std::vector<uint8_t> opus_buf(4000);
            int encoded = opus_encode_float(
                encoder, frame.data(), FRAME_SIZE,
                opus_buf.data(), (opus_int32)opus_buf.size());
            if (encoded <= 0) continue;

            auto   now = std::chrono::steady_clock::now().time_since_epoch();
            double ts = std::chrono::duration<double>(now).count();

            std::vector<uint8_t> packet(8 + 2 + encoded);
            memcpy(packet.data(), &ts, 8);
            uint16_t len = (uint16_t)encoded;
            memcpy(packet.data() + 8, &len, 2);
            memcpy(packet.data() + 10, opus_buf.data(), encoded);

            std::lock_guard<std::mutex> lock(Session::clients_mutex);
            for (auto& s : Session::clients) s->deliver(packet);
        }
        });

    // ── acceptループ: 接続ごとにスレッドを起動 ───────────────
    // handle_connectionを別スレッドで実行することで次のacceptをブロックしない
    boost::asio::io_context ioc;
    tcp::acceptor acceptor(ioc, { tcp::v4(), 9001 });
    std::cout << "Server is running on " << url << std::endl;

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        // 各接続を独立スレッドで処理 → acceptはすぐ次を待てる
        std::thread(handle_connection, std::move(socket)).detach();
    }

    opus_encoder_destroy(encoder);
    return 0;
}