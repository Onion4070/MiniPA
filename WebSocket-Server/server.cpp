#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <Windows.h>
#include <qrencode.h>

#include "audio_capture.h"
#include "net_utils.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients;
std::mutex clients_mutex;

std::string load_file(const std::string& path) {
    std::ifstream t(path);
    return { std::istreambuf_iterator<char>(t),{} };
}

void session(tcp::socket socket) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(socket, buffer, req);

    // WebSocket Upgrade判定
    if (websocket::is_upgrade(req) && req.target() == "/ws") {
        auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
        ws->accept(req);
		//tcp::no_delay option(true);
		//ws->next_layer().set_option(option);
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(ws);
		}
		std::cout << "WebSocket connection established." << std::endl;

        while (1) {
            try {
                beast::flat_buffer buf;
                ws->read(buf);
            }
            catch (beast::system_error const& se) {
                if (se.code() != websocket::error::closed) {
					//std::cerr << "server.cpp: void session()" << std::endl;
                    std::cerr << "Error: " << se.code().value() << std::endl;
                    break;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(
                std::remove(clients.begin(), clients.end(), ws),
                clients.end());
        }

		std::cout << "WebSocket connection closed." << std::endl;
        return;
    }

    // HTTP処理
    http::response<http::string_body> res{
        http::status::ok, req.version()
    };
    res.set(http::field::server, "Beast");
    res.set(http::field::content_type, "text/html");
    res.body() = load_file("index.html");
    res.prepare_payload();
    http::write(socket, res);
}

void show_qr(const char* text) {
    QRcode* qr = QRcode_encodeString(text, 0, QR_ECLEVEL_Q, QR_MODE_8, 1);
    if (!qr) {
        std::cerr << "Failed to generate QR code.\n";
        exit(1);
    }

    // 枠の幅
    const int border = 1;
    const int w = qr->width;
    unsigned char* data = qr->data;

    for (int y = -border; y < w + border; y += 2) {
        for (int x = -border; x < w + border; x++) {

            bool top = false;
            bool bottom = false;

            // 上ピクセル
            if (x >= 0 && y >= 0 && x < w && y < w)
                top = data[y * w + x] & 1;

            // 下ピクセル
            if (x >= 0 && y + 1 >= 0 && x < w && y + 1 < w)
                bottom = data[(y + 1) * w + x] & 1;

			// 最下段は下ピクセルなし
            if (y >= w + border - 1)
                bottom = true;

            // 白黒反転
            top = !top;
            bottom = !bottom;

            // 描画
            if (top && bottom)
                std::cout << (const char*)u8"█";
            else if (top)
                std::cout << (const char*)u8"▀";
            else if (bottom)
                std::cout << (const char*)u8"▄";
            else
                std::cout << " ";
        }
        std::cout << "\n";
    }

    QRcode_free(qr);
}

int main() {

    SetConsoleOutputCP(CP_UTF8);

    // http://{ server IP }:9001 のQRコードを表示
	std::string localIP = NetUtils::GetLocalIP();   
	std::string url = "http://" + localIP + ":9001";
	show_qr(url.c_str());

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

        std::lock_guard<std::mutex> lock(clients_mutex);

        for (auto ws : clients) {
            try {
                ws->binary(true);
                ws->write(boost::asio::buffer(packet));
            }
            catch (...) {}
        }
    });

    while (1) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;
        std::thread(session, std::move(socket)).detach();
    }
}
