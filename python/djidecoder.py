#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun
from struct import pack


# Metadata in:
#   1. Sample rate
#   2. FC
#   3. Sample number
#   4. Time stamp relative start of buffer

# Decoding steps:
#   1.  Channelization
#   2.  Downsampling
#   3.  Low pass filtering
#   4.  Integer Frequency Offset, IFO
#   5.  Find STO, SCO and resample
#   6.  CFO estimate
#   7.  OFDM symbol extraction
#   8.  Channel estimate
#   9.  Equalize data carriers
#   10. Demodulate QPSK
#   11. Descramble
#   12. Apply turbo coder (w/ QPSK load bearing constants)
#   13. CRC check

# Metadata out:
#   1. FC
#   2. CFO
#   3. TOA
#   4. SNR
#   5. STO
#   6. SCO
#   7. Channel estimate
#   8. FEC corrected error list
#   9. CRC residual

class djidecoder:
    def __init__(self):
        self.frame_crc_fct = mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer
        self.droneid = {}
        self.droneid["carrier_spacing"]  = 15.0e3 # Hz
        self.droneid["data_carriers"]    = 600
        self.droneid["symbols"]          = 8
        self.droneid["zc_root_symbol_4"] = 600
        self.droneid["zc_root_symbol_6"] = 147
        self.droneid["cp_seq"]           = [1, 0, 0, 0, 0, 0, 0, 0, 1] # 1 is long
        self.droneid["models"]           = { 16: "Mavic Pro", 41: "Mavic 2", 61: "DJI FPV", 63: "Mini 2", 68: "Mavic 3" }
        self.taps4 = self.filter_taps(symbol=4)
        self.taps6 = self.filter_taps(symbol=6)
        self.long_cp_len = 80
        self.short_cp_len = 72
        self.ofdm_symbol_len = 1024

    def integer_frequency_offset(self, data):
        return None

    def demodulate():
        # samp_rate = 15.36e6
        # 
        return None

    def estimate_integer_freq(self, data, window_len=20):
        idx = np.min( self.first_baseband_sample(data))
        symbol4 = data[idx: idx + self.ofdm_symbol_len]
        S4 = np.fft.fftshift(np.fft.fft(symbol4))
        idx = np.argmax( np.abs(S4[self.ofdm_symbol_len // 2 - window_len: self.ofdm_symbol_len // 2 + window_len + 1]))
        return (self.ofdm_symbol_len + window_len) // 2 - idx - 1

    def first_baseband_sample(self,data):
        # Return index of first sample in baseband
        #  filter data with both symbols
        #  
        pre_symbol4_len = 3 * (self.short_cp_len + self.ofdm_symbol_len)
        pre_symbol6_len = 5 * (self.short_cp_len + self.ofdm_symbol_len)
        idx4 = np.argmax( np.abs(np.convolve(data, self.taps4))) - pre_symbol4_len
        idx6 = np.argmax( np.abs(np.convolve(data, self.taps6))) - pre_symbol6_len
        return (idx4, idx6)

    def filter(self, data, symbol=4):
        if symbol == 4:
            return np.convolve(data, self.taps4, 'full')
        elif symbol == 6:
            return np.convolve(data, self.taps6, 'full')
        else:
            return None

    def filter_taps(self, symbol=4):
        if symbol==3 or symbol==4:
            root = self.droneid["zc_root_symbol_4"]
        elif symbol==5 or symbol==6:
            root = self.droneid["zc_root_symbol_6"]
        else:
            return None

        n = 1024
        guard_carriers = n - self.droneid["data_carriers"]
        lguard = guard_carriers // 2

        zc_w = np.zeros(n, dtype=np.complex128)

        for idx,_ in enumerate(range(self.droneid["data_carriers"] + 1)):
            x = -np.pi * root * idx * (idx + 1.0) / (self.droneid["data_carriers"] + 1)
            zc_w[idx + lguard] = np.exp(1j * x)
        zc_w[n // 2] = 0.0 # Set DC to 0
        zc_w = np.fft.fftshift(zc_w)
        zc = np.fft.ifft(zc_w)
        return np.conj(zc)
