#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun

# Poly, Initial = 0x00, No reflect, FinalXOR = 0x00
# 1 added from the left to poly
compute_crc = mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer

libdt = ct.cdll.LoadLibrary("libdt.so")
libdt.dt_turbo_fwd.restype  = ct.c_uint32
libdt.dt_turbo_fwd.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
libdt.dt_turbo_rev.restype  = ct.c_uint64
libdt.dt_turbo_rev.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]

droneid = {}
droneid["carrier_spacing"]  = 15.0e3 # Hz
droneid["data_carriers"]    = 600
droneid["symbols"]          = 8
droneid["zc_root_symbol_4"] = 600
droneid["zc_root_symbol_6"] = 147
#droneid["cp_seq"]           = [1, 0, 0, 0, 0, 0, 0, 0, 1] # 1 is long
droneid["cp_seq"]           = [0, 0, 0, 0, 0, 0, 0, 1] # 1 is long

def bits_to_qpsk(bits):
    N = len(bits)
    if N % 2 != 0:
        return None

    symbols = np.ndarray(N // 2, dtype=np.complex128)

    s = 0
    for idx in range(0, N, 2):
        b = (bits[idx], bits[idx + 1]) 
        if    b == (0, 0):
            symbols[s] = +1. + 1.j
        elif  b == (0, 1):
            symbols[s] = +1. - 1.j
        elif  b == (1, 0):
            symbols[s] = -1. + 1.j
        else:#b == (1, 1)
            symbols[s] = -1. - 1.j
        s += 1                       
    return np.sqrt(.5) * symbols

def baseband(qpsk_symbols, samp_rate):
    N_sym = droneid["symbols"]
    L_sym = droneid["data_carriers"]
    N_fft = fft_size(samp_rate)

    ofdm_symbols = np.zeros((N_sym, L_sym), dtype=np.complex128)
    ofdm_symbols_freq = np.zeros((N_sym, N_fft), dtype=np.complex128)
    ofdm_symbols_time = np.zeros((N_sym, N_fft), dtype=np.complex128)

    idx = 0
    for s in range(N_sym):
        if s == 2 or s == 4: #Symbol #3 and #5 carries ZC data
            pass
        else:
            ofdm_symbols[s, :] = qpsk_symbols[idx: idx + L_sym]
            idx += L_sym

    data_idx = data_indices(samp_rate)

    for s in range(N_sym):
        if   s == 2: # Symbol #3
            ofdm_symbols_time[s, :] = create_zc_sequence(samp_rate, symbol=3)
        elif s == 4: # Symbol #5
            ofdm_symbols_time[s, :] = create_zc_sequence(samp_rate, symbol=5)
        else:        # OFDM symbols carrying data
            ofdm_symbols_freq[s, data_idx] = ofdm_symbols[s, :]
            s_t = np.fft.ifft( np.fft.fftshift( ofdm_symbols_freq[s, :] ))
            #s_t /= N_fft
            ofdm_symbols_time[s, :] = s_t

    cp_l = long_cp_size(samp_rate)
    cp_s = short_cp_size(samp_rate)
    cp_schedule = [cp_l if x == 1 else cp_s for x in droneid["cp_seq"]]

    bb_len  = sum(cp_schedule)
    bb_len += N_fft * droneid["symbols"]
    iq = np.zeros( bb_len, dtype=np.complex128)

    # Add cyclic prefix
    idx = 0
    for s in range(N_sym):
        cp_len = cp_schedule[s]
        cp = ofdm_symbols_time[s, -cp_len:]
        sym = ofdm_symbols_time[s, :]
        iq[idx: idx + cp_len + N_fft] = np.concatenate([cp, sym])
        idx += cp_len + N_fft
    return iq

def data_indices(samp_rate):
    N_c = droneid["data_carriers"]
    N_fft  =fft_size(samp_rate)
    idx_dc = N_fft // 2
    N_left = N_c // 2
    N_right = N_c // 2
    # Build a list of indices, DC excluded
    indices =  [i for i in range(idx_dc - N_left, idx_dc)              ]
    indices += [i for i in range(idx_dc + 1     , idx_dc + N_right + 1)]
    return indices

def create_zc_sequence(samp_rate, symbol=3):
    # DJI OFDM settings
    if samp_rate < 15.36e6:
        return None

    if symbol==3 or symbol==4:
        root = droneid["zc_root_symbol_4"]
    elif symbol==5 or symbol==6:
        root = droneid["zc_root_symbol_6"]
    else:
        return None

    n = fft_size(samp_rate)
    guard_carriers = n - droneid["data_carriers"]
    lguard = guard_carriers // 2

    zc_w = np.zeros(n, dtype=np.complex128)

    for idx,_ in enumerate(range(droneid["data_carriers"] + 1)):
        x = -np.pi * root * idx * (idx + 1.0) / (droneid["data_carriers"] + 1)
        zc_w[idx + lguard] = np.exp(1j * x)
    zc_w[n // 2] = 0.0 # Set DC to 0
    zc_w = np.fft.fftshift(zc_w)
    zc = np.fft.ifft(zc_w)
    return zc #np.conj(zc)    

def fft_size(samp_rate):
    return int(np.round(samp_rate / droneid["carrier_spacing"]))

def short_cp_size(samp_rate):
    return int(np.round(samp_rate * 0.0000046875))

def long_cp_size(samp_rate):
    return int(np.round(samp_rate / 192000.0))

def dji_crc(data):
    res = compute_crc(data)
    crc = np.ndarray(3, dtype=np.uint8)
    crc[0] = ((res >> 16) & 0xff);
    crc[1] = ((res >>  8) & 0xff);
    crc[2] = ((res >>  0) & 0xff);
    return crc

def encode(msg_dict=None):
    if msg_dict is None:
        msg = np.array([ 88,  16,   2,   0,   0,   0,   0,  48,  49,  50,  51,  52,  53,\
            54,  55,  56,  57,  97,  98,  99, 100,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,  19,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  76, 201], dtype=np.uint8) #Some known good frame
    else:
        msg = np.ndarray(91, dtype=np.uint8)
        msg.fill(0xff)   

    scrap = np.ndarray(82, dtype=np.uint8)
    scrap.fill(0x00)

    msg = np.concatenate([msg, scrap])
    crc = dji_crc(msg)
    return np.concatenate([msg, crc]) # 176 CRC augmented payload bytes

def turbo_fwd(msg):
    if len(msg) != 176:
        return None
    frm = np.zeros(7200, dtype=np.uint8)
    res = libdt.dt_turbo_fwd(frm.ctypes.data_as(ct.POINTER(ct.c_uint8)), msg.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    return frm

def turbo_rev(frame):
    if len(frame) != 7200:
        return None
    msg = np.ndarray(176, dtype=np.uint8)
    res = libdt.dt_turbo_rev(msg.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))

    status = np.int32(np.uint32(res >> 32))
    crc = res & 0xffffffff
    return msg

def scramble(bits, gs=None):
    M_pn = 7200
    if len(bits) != M_pn:
        return None
    elif gs is None:
        return None
    else:
        for idx in range(M_pn):
            bits[idx] ^= gs[idx]
        return bits

def golden_sequence(x2_init=None):
    x1_init = np.array([1]+[0,]*30, dtype=np.uint8)

    if x2_init is None:
        # 0x12345678 in reverse order. Magic. Leave out leading 0.
        x2_init = np.array([0,0,1,0,0,1,0,0,0,1,1,0,1,0,0,0,1,0,1,0,1,1,0,0,1,1,1,1,0,0,0], dtype=np.uint8)
        x2_init = x2_init[::-1]
    elif len(x2_init) != len(x1_init):
        return None

    reg_len = len(x1_init)
    M_pn = 7200 # As defined in 36.211 7.2
    Nc = 1600 

    x1 = np.zeros(Nc + M_pn + len(x1_init), dtype=np.uint8)
    x2 = np.zeros(Nc + M_pn + len(x2_init), dtype=np.uint8)
    gs = np.zeros(M_pn, dtype=np.uint8)

    x1[:reg_len] = x1_init
    x2[:reg_len] = x2_init

    for idx in range(M_pn + Nc):
        x1[idx + reg_len] = (x1[idx + 3] + x1[idx]) % 2
        x2[idx + reg_len] = (x2[idx + 3] + x2[idx + 2] + x2[idx + 1] + x2[idx]) % 2

    for idx in range(M_pn):
        gs[idx] = (x1[idx + Nc] + x2[idx + Nc]) % 2
    return gs

# To modulate:
#  1. Encode 91 bytes from drone information              WAIT
#  2. Compute and add CRC                                 DONE
#  3. Turbo encode and rate match                         DONE
#  4. Run scrambler                                       DONE
#  5. Create OFDM symbols                                 TODO
#     a. Generate QPSK symbols                            DONE
#     b. Convert to time domain                           TODO
#     c. Set symbol 4 and 6 to required ZC sequences      DONE
#     d. Add cyclic prefix                                TODO



if __name__ == '__main__':
    gs = golden_sequence()
    s = ""
    for i in range(32):
        s += "{:2d} ".format(gs[i])
    print(s)
    exit()


    frame = np.array([ 88,  16,   2,   0,   0,   0,   0,  48,  49,  50,  51,  52,  53,\
        54,  55,  56,  57,  97,  98,  99, 100,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,  19,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  76, 201], dtype=np.uint8)
    frame.tofile("frame.orig.bin")
    out = np.zeros(7200, dtype=np.uint8)

    res = libdt.dt_turbo_fwd(out.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    out.tofile("frame.encoded.bin")
    print("Encoder CRC: {:02x}".format(res))

    decoded_frame = np.ndarray(176, dtype=np.uint8)
    encoded_frame = np.fromfile("frame.encoded.bin",dtype=np.uint8)

    # Introduce some bit errors
    no_bit_errors = 1000
    for idx in sample(range(0,7200), no_bit_errors):
        encoded_frame[idx] ^= 0x01

    res = libdt.dt_turbo_rev(decoded_frame.ctypes.data_as(ct.POINTER(ct.c_uint8)), encoded_frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    status = np.int32(np.uint32(res >> 32)) # re-cast to int32
    crc = res & 0xffffffff

    decoded_frame.tofile("frame.decoded.bin")
    print("Decoder CRC: {:3d}   Status: {:02x}".format(status, crc))
