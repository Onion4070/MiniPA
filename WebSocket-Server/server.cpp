#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

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
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

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

    // http://{ server IP }:9001 のQRコードを表示
	std::string localIP = NetUtils::GetLocalIP();
    if (localIP.empty()) {
        std::cerr << "Failed to get local IP address.\n";
        return 1;
	}

	std::string url = "http://" + localIP + ":9001";
	TerminalQR::show(url.c_str());

    boost::asio::io_context ioc;
    tcp::acceptor acceptor(ioc, { tcp::v4(),9001 });

	std::cout << "Server is running on " << url << std::endl;

    AudioCapture cap;
    cap.start([](const uint8_t* data, size_t size) {
        // 送信時タイムスタンプ
		auto now = std::chrono::steady_clock::now().time_since_epoch();
        double ts = std::chrono::duration<double>(now).count();

        // 送信パケット(タイムスタンプ + 音声)
        std::vector<uint8_t> packet(sizeof(double) + size);
		memcpy(packet.data(), &ts, sizeof(double));
		memcpy(packet.data() + sizeof(double), data, size);

        std::lock_guard<std::mutex> lock(Session::clients_mutex);
        for (auto& s : Session::clients) s->deliver(packet); // 音声データを各Sessionに送る
    });

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        
        // 各接続を独立スレッドで処理し，acceptはすぐ次を待てる
        std::thread(handle_connection, std::move(socket)).detach();
    }
}
