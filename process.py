import soundcard as sc
import subprocess
import numpy as np

# 設定
samplerate = 48000
channels = 2
blocksize = 960  # 20ms（低遅延のための標準的なサイズ）

# FFmpegプロセスの起動
# ユーザーのテスト用コマンドのパラメータを、実音源入力用に調整しています
ff_command = [
    'ffmpeg',
    '-y',
    '-re',                       # リアルタイム読み込み
    '-f', 'f32le',               # Pythonからの入力形式
    '-ar', str(samplerate),
    '-ac', str(channels),
    '-i', '-',                   # 標準入力から受け取る
    '-c:a', 'libopus',
    '-application', 'lowdelay',
    '-b:a', '64k',               # 指定のビットレート
    '-f', 'rtp',                 # RTP形式を指定
    'rtp://127.0.0.1:5000'       # 送信先
]

# プロセス開始
proc = subprocess.Popen(ff_command, stdin=subprocess.PIPE)

print("--- Streaming Desktop Audio via RTP ---")
print("Target: rtp://127.0.0.1:5000")
print("Press Ctrl+C to stop.")

try:
    # ループバック（デスクトップ音声）キャプチャの開始
    default_mic = sc.get_microphone(id=sc.default_speaker().name, include_loopback=True)
    with default_mic.recorder(samplerate=samplerate, channels=channels) as mic:
        while True:
            # 音声データの取得
            data = mic.record(numframes=blocksize)
            
            # FFmpegの標準入力にバイナリとして流し込む
            # soundcardはfloat64で返すことがあるため、f32leに合わせて変換
            proc.stdin.write(data.astype(np.float32).tobytes())
            
except KeyboardInterrupt:
    print("\nStreaming stopped.")
finally:
    if proc.stdin:
        proc.stdin.close()
    proc.terminate()