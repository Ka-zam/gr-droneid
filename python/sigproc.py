#!/usr/bin/python3
import numpy as np
from scipy.signal import decimate as scidec
from sys import argv, exit

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
    w = 20. * np.log10(np.abs(np.fft.fftshift(np.fft.fft(data))))
    return (f, w)

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