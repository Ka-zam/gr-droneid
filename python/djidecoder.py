#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun
from struct import pack
from sys import argv, exit


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
        self.droneid["cp_seq"]           = [80, 72, 72, 72, 72, 72, 72, 72, 80] # 1 is long
        self.droneid["models"]           = { 16: "Mavic Pro", 41: "Mavic 2", 61: "DJI FPV", 63: "Mini 2", 68: "Mavic 3" }
        self.long_cp_len = 80
        self.short_cp_len = 72
        self.ofdm_symbol_len = 1024
        self.data_carriers = 600
        self.taps4 = self.filter_taps(symbol=4)
        self.taps6 = self.filter_taps(symbol=6)
        self.indices = self.data_indices()
        self.baud_rate = 15.36e6

    def demodulate_hard(self, qpsk_syms):
        #bits = 
        # samp_rate = 15.36e6
        # 
        return None

    def qpsk_symbols(self, fd_syms):
        for r in range( fd_syms.shape[0]):
            syms[r,:] = fd_syms[r,self.indices]
        return syms

    def frequency_domain_symbols(self, td_syms):
        syms = np.zeros((self.droneid["symbols"], self.ofdm_symbol_len), dtype=np.complex128)
        for r in range( td_syms.shape[0]):
            syms[r,:] = np.fft.fftshift( np.fft.fft(td_syms[r,:]))
        return syms

    def time_domain_symbols(self, data):
        # assumes start of waveform is at data[0]
        syms = np.zeros((self.droneid["symbols"], self.ofdm_symbol_len), dtype=np.complex128)
        cp_seq = self.droneid["cp_seq"][-self.droneid["symbols"]:]
        idx = 0
        for s in range(self.droneid["symbols"]):
            idx += cp_seq[s]
            syms[s,:] = data[idx: idx + self.ofdm_symbol_len]
            idx += self.ofdm_symbol_len
        return syms

    def integer_frequency_estimate(self, data, start_idx, window=20):
        if window < 1:
            return None
        s4 = data[start_idx: start_idx + self.ofdm_symbol_len]
        S4 = np.fft.fftshift(np.fft.fft(s4))
        dc_idx = self.ofdm_symbol_len // 2
        idx = np.argmin(np.abs(S4[dc_idx - window : dc_idx + window]))
        return idx - window

    def fractional_frequency_estimate(self, data, start_idx):
        symbol4_idx = 3 * (self.short_cp_len + self.ofdm_symbol_len)
        start_idx += symbol4_idx
        s4 = data[start_idx: start_idx + self.ofdm_symbol_len + self.short_cp_len]
        cp_pre = s4[:self.short_cp_len]
        cp_post = s4[-self.short_cp_len:]
        offset_rad = np.angle( np.dot(cp_pre, np.conj(cp_post))) / self.ofdm_symbol_len
        offset_hz = offset_rad * self.baud_rate / (2. * np.pi)
        return offset_rad

    def first_baseband_sample(self, data):
        # Return index of first sample in baseband
        #  filter data with both symbols
        #  
        symbol4_idx = 3 * (self.short_cp_len + self.ofdm_symbol_len)
        symbol6_idx = 5 * (self.short_cp_len + self.ofdm_symbol_len)
        idx4_est = np.argmax( np.abs(np.convolve(data, self.taps4))) - symbol4_idx
        idx6_est = np.argmax( np.abs(np.convolve(data, self.taps6))) - symbol6_idx
        return (idx4_est, idx6_est)

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

        n = self.ofdm_symbol_len
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

    def data_indices(self):
        idx_dc = self.ofdm_symbol_len // 2
        N_left = self.droneid["data_carriers"] // 2
        N_right = self.droneid["data_carriers"] // 2
        # Build a list of indices, DC excluded
        indices  = [i for i in range(idx_dc - N_left, idx_dc)              ]
        indices += [i for i in range(idx_dc + 1     , idx_dc + N_right + 1)]
        return indices        

if __name__ == '__main__':
    #import matplotlib.pyplot as plt
    if len(argv) == 1:
        print("Usage:\n {} iq.fc64".format(argv[0]))
        exit(0)
    try:
        data = np.fromfile(argv[1],dtype=np.complex128)
    except:
        print("Could not open file {}".format(argv[1]))
    d = djidecoder()
    #plt.plot(np.real(data))
    #plt.show()
    idx = d.first_baseband_sample(data)
    print("Baseband start detected at: {}".format(idx))
    ifo = d.estimate_integer_freq(data,np.min(idx))
    print("Integer frequency offset: {}".format(ifo))
    #(cfo, data)


