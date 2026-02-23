#pragma once
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

class NetUtils
{
public:
	static std::string GetLocalIP();

private:
	static bool isVirtualAdapter(PIP_ADAPTER_ADDRESSES p);
};

