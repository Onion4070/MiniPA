#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>
#include <thread>

using tcp = boost::asio::ip::tcp;

int main() {
	const auto address = boost::asio::ip::make_address("127.0.0.1");
	const auto port = static_cast<unsigned short>(std::atoi("9001"));

	boost::asio::io_context ioc{ 1 }; // number of threads to run

	tcp::acceptor acceptor{ ioc, {address, port} };

	while (1) {
		tcp::socket socket{ ioc };
		acceptor.accept(socket);
		std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

		std::thread{[q = std::move(socket)]() mutable {
			boost::beast::websocket::stream<tcp::socket> ws {std::move(q)};

			// Accept the websocket handshake
			ws.accept();

			while (1) {
				// Receive a message from the client
				boost::beast::flat_buffer buffer;
				ws.read(buffer);

				auto out = boost::beast::buffers_to_string(buffer.cdata());
				std::cout << "Received: " << out << std::endl;
			}

		}}.detach();
	}

	return 0;
}