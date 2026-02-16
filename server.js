const express = require('express');
const http = require('http');
const dgram = require('dgram');
const { Server } = require('socket.io');
const { RTCPeerConnection, MediaStreamTrack, RTCRtpCodecParameters } = require('werift');
const qrcode = require('qrcode-terminal');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static('public'));

// 音声トラックを作成
const audioTrack = new MediaStreamTrack({ kind: 'audio' });

// UDPサーバー設定 (ffmpegからのパケットを受け取る)
const udpServer = dgram.createSocket('udp4');
udpServer.on('message', (msg) => {
    // 受け取ったRTPパケットをWebRTCトラックに直接流し込む
    audioTrack.writeRtp(msg);
});
udpServer.bind(5000);

io.on('connection', async (socket) => {
    console.log('Client connected');

    // 接続設定の修正
    const pc = new RTCPeerConnection({
        codecs: {
            audio: [
                new RTCRtpCodecParameters({
                    mimeType: "audio/opus",
                    clockRate: 48000,
                    channels: 2,
                    payloadType: 96 // デフォルト値として指定
                })
            ]
        }
    });

    // トラックを追加
    pc.addTrack(audioTrack);

    socket.on('offer', async (sdp) => {
        await pc.setRemoteDescription(sdp);
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        socket.emit('answer', pc.localDescription);
    });

    socket.on('disconnect', () => {
        pc.close().catch(console.error);
    });
});

server.listen(3000, '0.0.0.0', async () => {
    const { internalIpV4 } = await import('internal-ip');
    const ip = await internalIpV4() || 'localhost';
    const url = `http://${ip}:3000`;
    console.log(url);
    qrcode.generate(url, { small: true }, (code) => {
        console.log(code);
    });
});