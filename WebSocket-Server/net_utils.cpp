#include "net_utils.h"
#include <iostream>
#include <ws2tcpip.h>


std::string NetUtils::GetLocalIP() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return "";

    // 1.1.1.1のDNSサーバ
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, "1.1.1.1", &dest.sin_addr);
    dest.sin_port = htons(53);

    // UDPで接続(パケットは出ない)
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    connect(s, (sockaddr*)&dest, sizeof(dest));

	// クリーンアップ関数
    auto cleanup = [&]() {
        closesocket(s);
        WSACleanup();
	};

	// 接続に使用されたローカルIPアドレスを取得
    sockaddr_in sin{};
	int len = sizeof(sin);
    if (getsockname(s, (sockaddr*)&sin, &len) != 0) {
        cleanup();
		return "";
	}

	// バイナリ形式のIPアドレスを文字列に変換
	char buf[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &sin.sin_addr, buf, sizeof(buf)) == NULL) {
		cleanup();
		return "";
	}

	std::string localIP = std::string(buf);
    cleanup();

	return localIP;
}