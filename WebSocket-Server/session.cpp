#include "session.h"
#include <iostream>
#include <algorithm>

std::vector<std::shared_ptr<Session>> Session::clients;
std::mutex Session::clients_mutex;

Session::Session(tcp::socket socket) : socket_(std::move(socket)) {}

Session::~Session() {
    stop();
}

void Session::stop() {
    if (stop_threads_.exchange(true)) return;
    cv_.notify_all();
    if (write_thread_.joinable()) write_thread_.join();
}

void Session::run(http::request<http::string_body> req) {
    if (websocket::is_upgrade(req) && req.target() == "/ws") {
        ws_ = std::make_unique<websocket::stream<tcp::socket>>(std::move(socket_));
        ws_->accept(req);

        beast::get_lowest_layer(*ws_).set_option(tcp::no_delay(true));

        // self をキャプチャして、スレッド実行中に Session が消えないようにする
        auto self = shared_from_this();
        write_thread_ = std::thread([self]() {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            self->send_loop();
        });

        {
            std::lock_guard<std::mutex> lock(Session::clients_mutex);
            Session::clients.push_back(shared_from_this());
        }

        std::cout << "WebSocket connection established." << std::endl;

        while (true) {
            try {
                beast::flat_buffer buf;
                ws_->read(buf);
            }
            catch (std::exception& e) {
                std::cerr << "RUN EXCEPTION: " << e.what() << std::endl;
                break;
            }
        }

        stop();

        {
            std::lock_guard<std::mutex> lock(Session::clients_mutex);
            Session::clients.erase(std::remove(Session::clients.begin(), Session::clients.end(), shared_from_this()), Session::clients.end());
        }
        std::cout << "WebSocket connection closed." << std::endl;
    }
}

void Session::deliver(std::vector<uint8_t> data) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (send_queue_.size() >= 10) send_queue_.pop(); // 古いデータを捨てる
    send_queue_.push(std::move(data));
    cv_.notify_one();
}

void Session::send_loop() {
    while (!stop_threads_) {
        std::vector<uint8_t> packet;
        {
            std::unique_lock<std::mutex> lock(send_mutex_);
            cv_.wait(lock, [this] { return !send_queue_.empty() || stop_threads_; });
            if (stop_threads_) return;
            packet = std::move(send_queue_.front());
            send_queue_.pop();
        }
        // ws_が有効かつopenの場合のみ送信、例外で即終了
        try {
            if (ws_ && ws_->is_open()) {
                ws_->binary(true);
                ws_->write(boost::asio::buffer(packet));
            }
        }
        catch (std::exception& e) {
            std::cerr << "SEND_LOOP EXCEPTION: " << e.what() << std::endl;
            stop_threads_ = true; // エラー時はループを終了
            return;
        }
    }
}