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
        self.channel4 = np.fft.fftshift( np.fft.fft(np.conj(self.taps4))) # Need the actual ZC seq
        #self.channel4 /= np.max(np.abs(self.channel4))
        self.channel6 = np.fft.fftshift( np.fft.fft(np.conj(self.taps6))) # Need the actual ZC seq
        #self.channel6 /= np.max(np.abs(self.channel6))
        self.indices = self.data_indices()
        self.gs = self.golden_sequence()
        self.baud_rate = 15.36e6
        self.libdt = ct.cdll.LoadLibrary("libdt.so")
        self.libdt.dt_turbo_rev.restype  = ct.c_uint64
        self.libdt.dt_turbo_rev.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
        self.libdt.dt_turbo_rev_soft.restype  = ct.c_uint64
        self.libdt.dt_turbo_rev_soft.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_int8)]
    
    def turbo_rev_soft(self, frame):
        if len(frame) != 7200:
            return None
        elif type(frame) != np.int8:
            return None
        msg = np.ndarray(176, dtype=np.uint8)
        res = self.libdt.dt_turbo_rev_soft(msg.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_int8)))
        status = np.int32(np.uint32(res >> 32))
        crc = res & 0xffffffff
        return msg

    def turbo_rev_hard(self, frame):
        if len(frame) != 7200:
            return None
        elif type(frame) != np.ndarray and frame.dtype != np.uint8:
            return None
        msg = np.ndarray(176, dtype=np.uint8)
        res = self.libdt.dt_turbo_rev(msg.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))
        status = np.int32(np.uint32(res >> 32))
        crc = res & 0xffffffff
        # Expect this to be 0x00 for passing CRC
        #print("libdt CRC: {:02x}".format(crc)) 
        return msg       

    def demodulate_qpsk_soft(self, qpsk_syms):
        # Scale to TurboFEC integer
        bits = np.ndarray(7200, dtype=np.int8)
        int_max = 63

        ptr = 0
        for s in range(qpsk_syms.shape[0]):
            if s == 2 or s == 4:
                pass
            else:
                scale = 1. / (np.std(qpsk_syms[s,:]) * np.sqrt(.5))
                qpsk_syms[s,:] *= scale
                for q in qpsk_syms[s,:]:
                    bits[ptr] = np.int8(np.round( -np.real(q) * int_max))
                    ptr += 1
                    bits[ptr] = np.int8(np.round( -np.imag(q) * int_max))
                    ptr += 1

        return bits.clip(-int_max, int_max)

    def demodulate_qpsk_hard(self, qpsk_syms):
        bits = np.ndarray(7200, dtype=np.uint8)
        ptr = 0
        for s in range(qpsk_syms.shape[0]):
            if s == 2 or s == 4:
                pass
            else:
                for q in qpsk_syms[s,:]:
                    bits[ptr] = np.real(q) < 0.
                    ptr += 1
                    bits[ptr] = np.imag(q) < 0.
                    ptr += 1
        return bits

    def qpsk_symbols(self, fd_syms):
        syms = np.zeros((self.droneid["symbols"], len(self.indices)), dtype=np.complex128)
        for r in range( fd_syms.shape[0]):
            syms[r,:] = fd_syms[r,self.indices]
        return syms

    def channel_equalize(self, qpsk_syms, channel_estimate):
        for r in range( qpsk_syms.shape[0]):
            qpsk_syms[r,:] *= channel_estimate
        return qpsk_syms        

    def channel_estimate(self,data_fd, symbol=4, method='naive'):
        if symbol == 4:
            return (self.channel4 / (data_fd + 1.e-10))[self.indices]
        elif symbol == 6:
            return (self.channel6 / (data_fd + 1.e-10))[self.indices]
        else:
            return np.ones(len(self.indices), dtype=np.complex128)

    def frequency_domain_symbols(self, td_syms):
        # Return frequency domain symbols
        syms = np.zeros((self.droneid["symbols"], self.ofdm_symbol_len), dtype=np.complex128)
        for r in range( td_syms.shape[0]):
            syms[r,:] = np.fft.fftshift( np.fft.fft(td_syms[r,:]))
        return syms

    def time_domain_symbols(self, data):
        # Return time domain samples sans CP
        # assumes start of waveform is at data[0]
        syms = np.zeros((self.droneid["symbols"], self.ofdm_symbol_len), dtype=np.complex128)
        cp_seq = self.droneid["cp_seq"][-self.droneid["symbols"]:]
        idx = 0
        for r in range(self.droneid["symbols"]):
            idx += cp_seq[r]
            #print("symbol: {}   idx: {}".format(r,idx))
            syms[r,:] = data[idx: idx + self.ofdm_symbol_len]
            idx += self.ofdm_symbol_len
        return syms

    def integer_frequency_estimate(self, data, start_idx=0, window=20):
        if window < 1:
            return None
        first_zc_idx = 2 * (self.short_cp_len + self.ofdm_symbol_len) + self.short_cp_len
        start_idx += first_zc_idx
        first_zc_td = data[start_idx: start_idx + self.ofdm_symbol_len]
        first_zc_fd = np.fft.fftshift(np.fft.fft(first_zc_td))
        dc_idx = self.ofdm_symbol_len // 2
        idx = np.argmin(np.abs(first_zc_fd[dc_idx - window : dc_idx + window]))
        return idx - window

    def fractional_frequency_estimate(self, data, start_idx=0):
        first_zc_idx = 2 * (self.short_cp_len + self.ofdm_symbol_len)
        start_idx += first_zc_idx
        first_zc_td = data[start_idx: start_idx + self.short_cp_len + self.ofdm_symbol_len ]
        cp_pre = first_zc_td[:self.short_cp_len]
        cp_post = first_zc_td[-self.short_cp_len:]
        offset_rad = np.angle( np.dot(cp_pre, np.conj(cp_post))) / self.ofdm_symbol_len
        offset_hz = offset_rad * self.baud_rate / (2. * np.pi)
        return offset_rad

    def first_baseband_sample(self, data):
        # Return index of first sample in baseband
        #  filter data with both symbols
        first_zc_idx = 2 * (self.short_cp_len + self.ofdm_symbol_len)
        second_zc_idx = 4 * (self.short_cp_len + self.ofdm_symbol_len)
        idx4_est  = np.argmax( np.abs(np.convolve(data, self.taps4)))
        idx4_est -= first_zc_idx + len(self.taps4) + self.short_cp_len
        idx6_est  = np.argmax( np.abs(np.convolve(data, self.taps6)))
        idx6_est -= second_zc_idx + len(self.taps6) + self.short_cp_len
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

    def descramble(self, bits):
        if len(bits) != len(self.gs):
            return None
        else:
            des = np.copy(bits)
            for idx in range(len(self.gs)):
                des[idx] ^= self.gs[idx]
            return des

    def golden_sequence(self, x2_init=None):
        x1_init = bytes([1,] + [0,] * 30)
        if x2_init is None:
            # 0x12345678 in reverse order. Magic. Leave out leading 0.
            x2_init = bytes([0,0,1,0,0,1,0,0,0,1,1,0,1,0,0,0,1,0,1,0,1,1,0,0,1,1,1,1,0,0,0])
            x2_init = x2_init[::-1]
        elif len(x2_init) != len(x1_init):
            return None

        reg_len = len(x1_init)
        M_pn = 7200 # As defined in 36.211 7.2
        Nc = 1600 

        x1 = bytearray(Nc + M_pn + len(x1_init))
        x2 = bytearray(Nc + M_pn + len(x2_init))
        gs = bytearray(M_pn)

        x1[:reg_len] = x1_init
        x2[:reg_len] = x2_init

        for idx in range(M_pn + Nc):
            x1[idx + reg_len] = (x1[idx + 3] + x1[idx]) % 2
            x2[idx + reg_len] = (x2[idx + 3] + x2[idx + 2] + x2[idx + 1] + x2[idx]) % 2

        for idx in range(M_pn):
            gs[idx] = (x1[idx + Nc] + x2[idx + Nc]) % 2
        return gs

    def decode(self, rx_dict, equalize=False):
        try:
            data = rx_dict["signal"]
        except Exception as e:
            raise e

        idx = self.first_baseband_sample(data)
        if idx[0] != idx[1]:
            return None
        # Strip data
        data = data[idx[0]:]
        idx = self.first_baseband_sample(data)
        ifo = self.integer_frequency_estimate(data)    
        ffo = self.fractional_frequency_estimate(data)
        syms_td = self.time_domain_symbols(data)
        syms_fd = self.frequency_domain_symbols(syms_td)
        syms_qpsk = self.qpsk_symbols(syms_fd)    
        if equalize:
            ch4_est = self.channel_estimate(syms_fd[2,:], 'naive')
            self.channel_equalize(syms_qpsk, ch4_est)

        raw_bits = self.demodulate_qpsk_hard(syms_qpsk)
        descrambled_bits = self.descramble(raw_bits)
        payload = bytes(self.turbo_rev_hard(descrambled_bits))
        res = {}
        res["payload"] = payload
        res["raw_bits"] = raw_bits
        res["descrambled_bits"] = descrambled_bits
        res["start_idx"] = idx
        return res

if __name__ == '__main__':
    import matplotlib.pyplot as plt
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
    print("Baseband start detected at  : {}".format(idx))
    data = data[idx[0]:]
    
    ifo = d.integer_frequency_estimate(data)
    print("Integer frequency offset    : {}".format(ifo))
    
    ffo = d.fractional_frequency_estimate(data)
    print("Fractional frequency offset : {}".format(ffo))

    syms_td = d.time_domain_symbols(data)
    syms_fd = d.frequency_domain_symbols(syms_td)

    #plt.plot(np.real(data))
    #plt.show()

    syms_qpsk = d.qpsk_symbols(syms_fd)

    if True: # Do channel equalization
        ch4_est = d.channel_estimate(syms_fd[2,:], 'naive')
        d.channel_equalize(syms_qpsk, ch4_est)

    hard_bits = d.demodulate_qpsk_hard(syms_qpsk)
    print("Demodulated bits            : {}".format(len(hard_bits)))
    hard_bits = d.descramble(hard_bits)
    print("Descrambled bits            : {}".format(len(hard_bits)))
    hard_msg = d.turbo_rev_hard(hard_bits)
    hard_msg = bytes(hard_msg)

    serial = hard_msg[7: 7 + 16]
    uuid = hard_msg[69: 69 + 19]
    rx_crc = hard_msg[-3:]
    cmp_crc = d.frame_crc_fct(hard_msg)
    print("Serial                      : >{:16s}<".format(serial.decode()))
    print("UUID                        : >{:19s}<".format(uuid.decode()))
    print("Received CRC                : {}".format(rx_crc.hex()))
    print("CRC Check                   : {}".format(["Failure", "Success", ][cmp_crc == 0]))
