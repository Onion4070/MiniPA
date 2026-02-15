document.getElementById("start").onclick = async () => {

    const pc = new RTCPeerConnection();

    /* ---------- サーバ音受信 ---------- */
    pc.ontrack = e => {
        const audio = new Audio();
        audio.srcObject = e.streams[0];
        audio.play();
    };

    /* ---------- ブラウザ音生成 ---------- */
    const ctx = new AudioContext();
    const osc = ctx.createOscillator();
    osc.start();

    const dst = ctx.createMediaStreamDestination();
    osc.connect(dst);

    dst.stream.getTracks().forEach(t => pc.addTrack(t));

    /* ---------- Offer ---------- */
    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);

    const answer = await fetch("/offer", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(offer)
    }).then(r => r.json());

    await pc.setRemoteDescription(answer);
};
