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

class session;
std::vector<std::shared_ptr<session>> clients;
std::mutex clients_mutex;

std::string load_file(const std::string& path) {
    std::ifstream t(path);
    return { std::istreambuf_iterator<char>(t),{} };
}

class session : public std::enable_shared_from_this<session> {
    tcp::socket socket_;
    std::shared_ptr<websocket::stream<tcp::socket>> ws_;
    std::vector<std::vector<uint8_t>> send_queue_;
    std::mutex send_mutex_;
    bool sending_ = false;

public:
    explicit session(tcp::socket socket) : socket_(std::move(socket)) {}

    void run(http::request<http::string_body> req) {
        if (websocket::is_upgrade(req) && req.target() == "/ws") {
            ws_ = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket_));
            ws_->accept(req);
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back(shared_from_this());
            }

            std::cout << "WebSocket connection established." << std::endl;

            while (true) {
                try {
                    beast::flat_buffer buf;
                    ws_->read(buf);
                }
                catch (beast::system_error const& se) {
                    if (se.code() != websocket::error::closed) std::cerr << "Error: " << se.code().value() << std::endl;
                    break;
                }
            }

            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.erase(std::remove(clients.begin(), clients.end(), shared_from_this()), clients.end());
            }

            std::cout << "WebSocket connection closed." << std::endl;
        }
    }

    void deliver(std::vector<uint8_t> data) {
        // 受け取ったデータをキューに溜める
        std::lock_guard<std::mutex> lock(send_mutex_);
        send_queue_.push_back(std::move(data));

        if (!sending_) {
            sending_ = true;
            std::thread([self = shared_from_this()]() {
                try {
                    while (true) {
                        // キューから順番に取り出す
                        std::vector<uint8_t> packet;
                        {
                            std::lock_guard<std::mutex> lock(self->send_mutex_);
                            if (self->send_queue_.empty()) {
                                self->sending_ = false;
                                return;
                            }
                            packet = std::move(self->send_queue_.front());
                            self->send_queue_.erase(self->send_queue_.begin());
                        }
                        // 自分のwebsocketからのみ送信
                        if (self->ws_ && self->ws_->is_open()) {
                            self->ws_->binary(true);
                            self->ws_->write(boost::asio::buffer(packet));
                        }
                    }
                }
                catch (...) {
                    std::lock_guard<std::mutex> lock(self->send_mutex_);
                    self->sending_ = false;
                }
                }).detach();
        }
    }
};

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
        for (auto& s : clients) s->deliver(packet); // 音声データを各sessionに送る
    });

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);
        std::cout << "Request target: " << req.target() << std::endl;
        std::cout << "Is upgrade: " << websocket::is_upgrade(req) << std::endl;

        if (websocket::is_upgrade(req) && req.target() == "/ws") {
            // 接続ごとに1インスタンス生成する
            auto s = std::make_shared<session>(std::move(socket));
            std::thread([s, req = std::move(req)]() mutable {s->run(std::move(req));}).detach();
        }
        else {
            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, "Beast");
            res.set(http::field::content_type, "text/html");
            res.body() = load_file("index.html");
            res.prepare_payload();
            http::write(socket, res);
        }
    }
}
