#include "terminal_qr.h"
#include <iostream>
#include <qrencode.h>
#include <Windows.h>

void TerminalQR::show(const char* text) {
    UINT cp = GetConsoleOutputCP();
	SetConsoleOutputCP(CP_UTF8);

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
	SetConsoleOutputCP(cp);
}
