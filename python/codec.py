#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun
from struct import pack
import datetime
now = datetime.datetime.now

libdt = ct.cdll.LoadLibrary("libdt.so")
libdt.dt_turbo_fwd.restype  = ct.c_uint32
libdt.dt_turbo_fwd.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
libdt.dt_turbo_rev.restype  = ct.c_uint64
libdt.dt_turbo_rev.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]

#################################################################################
#    FIELD      START  LEN  ENCODING          COMMENT
#################################################################################
# packet_len     0      1    uint8           Typically 88
# packet_type    1      1    uint8           Typically 16
# version        2      1    uint8           Typically 2
# sequence_num   3      2    le_int16        Running counter
# state_info     5      2    uint8[2]        unknown meaning, [0, 0]
# serial         7     16    uint8[16]       Serial string
# uav_lon       23      4    le_int32        UAV longitude [-180, 180], scaled 
# uav_lat       27      4    le_int32        UAV latitude [-90, 90], scaled
# uav_height    31      2    le_int16        UAV height in m
# uav_alt       33      2    le_int16        UAV altitude in m
# uav_vel_n     35      2    le_int16        UAV velocity North m/s
# uav_vel_e     37      2    le_int16        UAV velocity East
# uav_vel_u     39      2    le_int16        UAV velocity Up
# uav_yaw       41      2    le_int16        UAV yaw
# pilot_time    43      8    le_uint64       Pilot UNIX time in ms
# pilot_lat     51      4    le_int32        Pilot latitude [-90, 90], scaled 
# pilot_lon     55      4    le_int32        Pilot longitude [-180, 180], scaled 
# home_lon      59      4    le_int32        Home longitude [-180, 180], scaled 
# home_lat      63      4    le_int32        Home latitude [-90, 90], scaled
# product_type  67      1    uint8           DJI product type
# uuid_len      68      1    uint8           uuid length, max 19
# uuid          69     19    uint8[19]       uuid, 0x00 fill at end
# terminator    88      1    uint8           0x00
# payload_crc   90      2    uint8           payload crc
# scrap_bytes   91     82    uint8           Seems to be 'DONT CARE'
# frame_crc    173      3    uint8           frame crc
###################   176    uint8           ####################################
#################################################################################

droneid = {}
droneid["carrier_spacing"]  = 15.0e3 # Hz
droneid["data_carriers"]    = 600
droneid["symbols"]          = 8
droneid["zc_root_symbol_4"] = 600
droneid["zc_root_symbol_6"] = 147
droneid["cp_seq"]           = [1, 0, 0, 0, 0, 0, 0, 0, 1] # 1 is long
droneid["models"]           = { 16: "Mavic Pro", 41: "Mavic 2", 61: "DJI FPV", 63: "Mini 2", 68: "Mavic 3" }

# Poly, Initial = 0x00, No reflect, FinalXOR = 0x00
# 1 added from the left to poly
frame_crc_fct = mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer

def frame_crc(data):
    res = frame_crc_fct(data)
    crc = bytearray(3)
    crc[0] = ((res >> 16) & 0xff);
    crc[1] = ((res >>  8) & 0xff);
    crc[2] = ((res >>  0) & 0xff);
    return crc

def payload_crc(data):
    init = 0x3692
    table =(0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
            0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
            0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
            0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
            0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
            0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
            0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
            0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
            0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
            0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
            0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
            0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
            0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
            0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
            0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
            0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
            0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
            0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
            0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
            0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
            0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
            0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
            0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
            0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
            0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
            0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
            0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
            0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
            0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
            0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
            0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
            0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78)
    #print("{:04x}".format(table[128]))
    res = init
    for i in range(len(data)):
        res = (res >> 8) ^ table[(res ^ data[i]) & 0x00ff]
    crc = bytearray(2)
    crc[0] = ((res >>  0) & 0xff);
    crc[1] = ((res >>  8) & 0xff);
    return crc

def msg_to_baseband(msg_dict={}, samp_rate=15.36e6):
    frm = frame(msg_dict)
    bits = turbo_fwd(frm)
    gs = golden_sequence()
    bits = scramble(bits, gs)
    syms = bits_to_qpsk(bits)
    bb = baseband(syms, samp_rate)
    return bb

def msg_to_bits(msg_dict={}):
    frm = frame(msg_dict)
    bits = turbo_fwd(frm)
    gs = golden_sequence()
    bits = scramble(bits, gs)
    return bits

def msg_to_symbols(msg_dict={}):
    frm = frame(msg_dict)
    bits = turbo_fwd(frm)
    gs = golden_sequence()
    bits = scramble(bits, gs)
    syms = bits_to_qpsk(bits)
    return syms

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
    cp_schedule = [cp_l if x == 1 else cp_s for x in droneid["cp_seq"][-N_sym:]]

    bb_len  = sum(cp_schedule)
    bb_len += N_fft * droneid["symbols"]
    iq = np.zeros( bb_len, dtype=np.complex128)

    # Add cyclic prefix
    idx = 0
    for s in range(N_sym):
        cp_len = cp_schedule[s]
        #print("s: {}  cp_len: {}".format(s,cp_len))
        cp = ofdm_symbols_time[s, -cp_len:]
        sym = ofdm_symbols_time[s, :]
        iq[idx: idx + cp_len + N_fft] = np.concatenate([cp, sym])
        idx += cp_len + N_fft
    return iq / np.max(np.abs(iq))

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

def droneid_fields():
    k = ['packet_len']
    k.append('packet_type')
    k.append('version')
    k.append('sequence_num')
    k.append('state_info')
    k.append('serial')
    k.append('uav_lat')
    k.append('uav_lon')
    k.append('uav_height')
    k.append('uav_alt')
    k.append('uav_vel_n')
    k.append('uav_vel_e')
    k.append('uav_vel_u')
    k.append('uav_yaw')
    k.append('pilot_time')
    k.append('pilot_lat')
    k.append('pilot_lon')
    k.append('home_lat')
    k.append('home_lon')
    k.append('product_type')
    k.append('uuid_len')
    k.append('uuid')
    k.append('terminator')
    k.append('scrap')
    k.append('crc')
    return k

def droneid_defaults():
    msg_dict = {}
    msg_dict['packet_len'] = 88
    msg_dict['packet_type'] = 16
    msg_dict['version'] = 2
    msg_dict['sequence_num'] = 12345 #
    msg_dict['state_info'] = [0, 0]
    ser = b'Skysense'
    msg_dict['serial'] = ser + b'0' * (16 - len(ser))
    msg_dict['uav_lat'] = 59.401961877206965 # Österö
    msg_dict['uav_lon'] = 17.96186335631357
    msg_dict['uav_alt'] = 100
    msg_dict['uav_height']  = 89
    msg_dict['uav_vel_n'] = 0.123
    msg_dict['uav_vel_e'] = 1.234
    msg_dict['uav_vel_u'] = .02
    msg_dict['uav_yaw'] = 12
    msg_dict['pilot_time'] = 1668595129000 # 2022-11-16 11:39
    msg_dict['pilot_lat'] = 59.40108401242872 # Hauka
    msg_dict['pilot_lon'] = 17.95851152195049
    msg_dict['home_lat'] = 59.403246486946564 # Ericsson
    msg_dict['home_lon'] = 17.956683191685695
    msg_dict['product_type'] = 68 # Mavic 3
    msg_dict['uuid_len'] = 19
    msg_dict['uuid'] = b'0123456789abcdefghi'
    #msg_dict['terminator'] = 0x00
    return msg_dict

def frame(msg_dict={}):
    msg = droneid_defaults()
    for k in msg_dict.keys():
        try:
            msg[k] = msg_dict[k]
        except:
            pass

    scale = 10e6 * np.pi / 180.
    frame = bytearray(176)

    frame[0] = 88
    frame[1] = 16
    frame[2] =  2
    frame[3:3 + 2] = pack('<h', msg['sequence_num'])
    frame[5:5 + 2] = msg['state_info']
    frame[7:7 + 16] = msg["serial"][:16]
    lon = round(scale * msg["uav_lon"])
    frame[23:23 + 4] = pack('<i', lon)
    lat = round(scale * msg["uav_lat"])
    frame[27:27 + 4] = pack('<i', lat)
    frame[31:31 + 2] = pack('<h', round(msg["uav_height"] * 1.))
    frame[33:33 + 2] = pack('<h', round(msg["uav_alt"] * 10.))
    frame[35:35 + 2] = pack('<h', round(msg["uav_vel_n"] * 100.))
    frame[37:37 + 2] = pack('<h', round(msg["uav_vel_e"] * 100.))
    frame[39:39 + 2] = pack('<h', round(msg["uav_vel_u"] * 100.))
    frame[41:41 + 2] = pack('<h', round(msg["uav_yaw"] * 100.))
    frame[43:43 + 8] = pack('<Q', msg["pilot_time"])
    lat = round(scale * msg["pilot_lat"])
    frame[51:51 + 4] = pack('<i', lat)
    lon = round(scale * msg["pilot_lon"])
    frame[55:55 + 4] = pack('<i', lon)
    lon = round(scale * msg["home_lon"])
    frame[59:59 + 4] = pack('<i', lon)    
    lat = round(scale * msg["home_lat"])
    frame[63:63 + 4] = pack('<i', lat)
    frame[67] = msg["product_type"]
    uuid = msg["uuid"][:19]
    frame[68] = len(uuid)
    frame[69:69 + 19] = uuid + b'\x00' * (19 - len(uuid))
    frame[88] = 0x00
    frame[90:90 + 2] = payload_crc(frame[:89])
    # Scrap values could be set here...
    # frame[91:91 + 82] = bytearray(82, )
    frame[173:173 + 3] = frame_crc(frame[:173])
    return frame

def encode(msg_dict=None):
    if msg_dict is None:
        msg = bytearray([ 88,  16,   2,   0,   0,   0,   0,  48,  49,  50,  51,  52,  53,\
            54,  55,  56,  57,  97,  98,  99, 100,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,  19,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  76, 201]) #Some known good frame
    else:
        msg = bytearray([0xff for x in range(91)])

    scrap = bytearray(82)

    msg += scrap
    msg += frame_crc(msg)
    return msg # 176 CRC augmented payload bytes

def turbo_fwd(msg):
    if len(msg) != 176:
        return None
    else:
        msg = np.array(msg)
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

def good_frame():
    # just some Mavic 3 frame from the air
    frm = bytes.fromhex('5810021702371f334e5a434843513030343358563100000000000000000000000007000000ffff\
                         00003ec3c2f218d582010000e4319e0079d32f0000000000000000003f13313331353932343331\
                         32363839323333393230003fff0000000000000000000000000000000000000000000000000000\
                         0000000000000000000000000000000000000000000000000041c370e098e2a14c98d3d7a4299b\
                         083e24034308f64c0925093855742faffe04dd2a')
    return frm

def cnoise(num=100, pwr_db=10):
    a = 10**(pwr_db / 20) * np.sqrt(.5)
    return a* (np.random.randn(num) + 1j*np.random.randn(num))

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
