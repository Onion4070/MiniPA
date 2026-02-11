import soundcard as sc
import numpy as np

samplerate = 48000
blocksize  = 1920

default_mic = sc.get_microphone(id=sc.default_speaker().name, include_loopback=True)

with default_mic.recorder(samplerate=samplerate, channels=2) as mic:
    print("Recording...")
    while True:
        data = mic.record(numframes=blocksize)
        mag = np.linalg.norm(data)
        print(f'Volume: {mag:6.2f}', end='\r')