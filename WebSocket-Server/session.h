#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
    tcp::socket socket_;
    std::shared_ptr<websocket::stream<tcp::socket>> ws_;
    std::queue<std::vector<uint8_t>> send_queue_;
    std::mutex send_mutex_;
    std::condition_variable cv_;
    bool stop_threads_ = false;
    std::thread write_thread_;

public:
    static std::vector<std::shared_ptr<Session>> clients;
    static std::mutex clients_mutex;

    explicit Session(tcp::socket socket);
    ~Session();

    void run(http::request<http::string_body> req);
    void deliver(std::vector<uint8_t> data);
    void stop();

private:
    void send_loop();
};