import soundcard as sc
import numpy as np
import asyncio
import warnings
import struct
import time
import qrcode
import socket

from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse

# 音飛び時の警告を無視
warnings.filterwarnings("ignore", message="data discontinuity")

# 設定
SAMPLERATE = 48000
BLOCKSIZE  = 480

# モノラルでループバック録音
mic = sc.get_microphone(id=sc.default_speaker().name, include_loopback=True)
recorder = mic.recorder(samplerate=SAMPLERATE, channels=1)
recorder.__enter__()

# PCのスピーカー出力を取得
def capture_audio():
    data = recorder.record(numframes=BLOCKSIZE)
    return data.astype(np.float32).tobytes()

# ローカルIPアドレスを取得
def get_local_ip():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(('8.8.8.8', 80))
            return s.getsockname()[0]
    except Exception:
        return '127.0.0.1'

# コンソールにQRコードを表示する
def show_qr_code(url):
    qr = qrcode.QRCode()
    qr.add_data(url)
    qr.make(fit=True)
    print(f"URL: {url}")
    qr.print_ascii(invert=True)


app = FastAPI()

# HTMLファイルを返すエンドポイント
@app.get("/")
async def index():
    with open("index.html", "r", encoding="utf-8") as f:
        return HTMLResponse(f.read())

# WebSocketで音声データをストリーミング送信
@app.websocket("/ws")
async def audio_ws(ws: WebSocket):
    await ws.accept()
    loop = asyncio.get_event_loop()
    try:
        while True:
            # 録音処理を別スレッドで実行
            audio_bytes = await loop.run_in_executor(None, capture_audio)
            # 送信時刻（8バイト）+ 音声データを送信
            timestamp = struct.pack('d', time.time())
            await ws.send_bytes(timestamp + audio_bytes)
    except:
        pass

if __name__ == "__main__":
    import uvicorn
    show_qr_code(f"http://{get_local_ip()}:8000")
    uvicorn.run(app, host="0.0.0.0", port=8000)