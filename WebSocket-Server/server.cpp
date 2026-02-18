#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

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
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.accept(req);

        while (1) {
            beast::flat_buffer buf;
            ws.read(buf);
            ws.text(ws.got_text());
            ws.write(buf.data());
        }
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

    while (1) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread(session, std::move(socket)).detach();
    }
}
