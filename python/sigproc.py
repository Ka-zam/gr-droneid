#!/usr/bin/python3
import numpy as np
from scipy.signal import decimate as scidec
from scipy.signal import remez
from sys import argv, exit

def overlap_save(x, h, Nfft=None):
    Nfft = max(Nfft or x.size + h.size - 1, h.size)
    hf = np.conj(np.fft.fft(h, Nfft))

    nvalid = Nfft - h.size + 1
    y = np.empty_like(x)
    offset = 0
    chunk = np.zeros(Nfft, dtype=x.dtype)
    while offset < x.size:
        thislen = min(offset + Nfft, x.size) - offset
        chunk[thislen:] = 0
        chunk[:thislen] = x[offset:offset + thislen]
        chunk = np.fft.ifft(np.fft.fft(chunk) * hf)

        thisend = min(offset + nvalid, x.size) - offset
        if np.iscomplexobj(y):
            y[offset:offset + thisend] = chunk[:thisend]
        else:
            y[offset:offset + thisend] = chunk[:thisend].real

        offset += nvalid
    return y

def bpf_taps(num_taps=32, samp_rate=61.44e6):
    # 2399.5    2414.5    2429.5    2444.5    2459.5 
    #  rare 
    # Channel width 10 MHz, bladeRF analog bandwith is 56 MHz
    # 2464.5 - 2409.5 = 55 MHz
    # 4 channel case:
    #  FC = 2436.5, FS = 61.44e6
    #
    if np.isclose(samp_rate, 61.44e6):
        fc = 2437.e6
        f_ch = np.array([2414.5, 2429.5, 2444.5, 2459.5]) * 1e6
        f_nor = (f_ch - fc) / samp_rate

    band_edge_lo = 5.5e6 / samp_rate
    band_edge_hi = 8.0e6 / samp_rate
    lpf = remez(num_taps, [0., band_edge_lo, band_edge_hi, .5,], [1.,0.], type='bandpass')
    n = np.arange(-(num_taps - 1) / 2, (num_taps - 1) / 2 + 1)
    taps = []
    for f in f_nor:
        taps.append(lpf * np.exp(1j * 2 * np.pi * n * f))
    return taps

def ftranslate(data, f=0.0, samp_rate=15.36e6):
    T = 1. / samp_rate
    t = np.arange(0.,  T * len(data), T)
    e = np.exp(1j * 2. * np.pi * t * f)
    return data * e

def decimate(data, samp_rate=61.44e6):
    Q = int(samp_rate / 15.36e6)
    return scidec(data, Q, ftype = 'fir', zero_phase=True)

def spec(data, samp_rate=15.36e6):
    f = np.linspace(-samp_rate * .5 , samp_rate * .5, len(data))
    sxx = 20. * np.log10(np.abs(np.fft.fftshift(np.fft.fft(data))))
    return (f, sxx)

if __name__ == '__main__':
    import matplotlib.pyplot as plt
    if len(argv) == 1:
        print("Usage:\n {} file.fc32".format(argv[0]))
        exit(0)
    try:
        sample_ratio = int(argv[1])
        #sto = float(argv[2])
    except Exception as e:
        raise e    