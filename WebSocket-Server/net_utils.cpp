#include "net_utils.h"
#include <iostream>

#pragma warning(disable:4996)  // inet_ntoa deprecated 警告を無視

std::string NetUtils::GetLocalIP() {
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        std::cerr << "WSAStartup error\n";
        return std::string();
    }

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_INET; // IPv4
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;

    // サイズを取得
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    }
    if (pAddresses == NULL) {
        std::cerr << "malloc error\n";
        WSACleanup();
        return std::string();
    }

    DWORD dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
    if (dwRetVal != NO_ERROR) {
        std::cout << "GetAdaptersAddresses failed: " << dwRetVal << "\n";
        free(pAddresses);
        WSACleanup();
        return std::string();
    }

    // 有効なインターフェースのIPv4アドレスを探す
    const char* selectedIp = NULL;

    for (PIP_ADAPTER_ADDRESSES p = pAddresses; p != NULL; p = p->Next) {
        if (p->OperStatus != IfOperStatusUp) continue;
        if (isVirtualAdapter(p)) continue;

        for (PIP_ADAPTER_UNICAST_ADDRESS ua = p->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            SOCKADDR* addr = ua->Address.lpSockaddr;
            if (addr == NULL) continue;
            if (addr->sa_family == AF_INET) {
                SOCKADDR_IN* ipv4 = (SOCKADDR_IN*)addr;
                unsigned long ipnum = ntohl(ipv4->sin_addr.s_addr);
                unsigned char b1 = (ipnum >> 24) & 0xFF;
                unsigned char b2 = (ipnum >> 16) & 0xFF;

                // ループバックやAPIPA(169.254.x.x)は除外
                if (b1 == 127) continue;
                if (b1 == 169 && b2 == 254) continue;

                // IPv4アドレスを文字列に変換
                char* s = inet_ntoa(ipv4->sin_addr);
                if (s != NULL) {
                    //std::cout << s << "\n";
                    selectedIp = s;
                    break;
                }
            }
        }
        if (selectedIp) break;
    }

    if (!selectedIp) {
        std::cerr << "No active IPv4 address found\n";
        return std::string();
    }

    free(pAddresses);
    WSACleanup();
    return std::string(selectedIp);
}

bool NetUtils::isVirtualAdapter(PIP_ADAPTER_ADDRESSES p) {
    // IF_TYPEベースの仮想インターフェース判定
    if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) return true;   // ループバック
    if (p->IfType == IF_TYPE_TUNNEL)            return true;   // トンネル
    if (p->IfType == IF_TYPE_PPP)               return true;   // PPP
    if (p->IfType == IF_TYPE_PPP)               return true;   // PPPエンコーディング

    // フレンドリー名ベースの仮想インターフェース判定
    if (p->FriendlyName != NULL) {
        wchar_t nameLower[256];
        wcscpy_s(nameLower, sizeof(nameLower) / sizeof(wchar_t), p->FriendlyName);
        _wcslwr_s(nameLower, sizeof(nameLower) / sizeof(wchar_t));

        // 仮想化ソフトやVPNの一般的な名前
        if (wcsstr(nameLower, L"hyper-v") != NULL)    return true;
        if (wcsstr(nameLower, L"vmware") != NULL)     return true;
        if (wcsstr(nameLower, L"virtualbox") != NULL) return true;
        if (wcsstr(nameLower, L"docker") != NULL)     return true;
        if (wcsstr(nameLower, L"vpn") != NULL)        return true;
        if (wcsstr(nameLower, L"openstack") != NULL)  return true;
        if (wcsstr(nameLower, L"virtual") != NULL
            && wcsstr(nameLower, L"switch") != NULL)  return true;
    }

    return false;
}