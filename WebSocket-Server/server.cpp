#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

#include "audio_capture.h"

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
                    std::cerr << "Error: " << se.code().message() << std::endl;
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

int main() {
    boost::asio::io_context ioc;
    tcp::acceptor acceptor(ioc, { tcp::v4(),9001 });

	std::cout << "Server is running on port 9001..." << std::endl;

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
