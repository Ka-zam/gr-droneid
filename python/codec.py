#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun


libdt = ct.cdll.LoadLibrary("libdt.so")
libdt.dt_turbo_fwd.restype  = ct.c_uint32
libdt.dt_turbo_fwd.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
libdt.dt_turbo_rev.restype  = ct.c_uint64
libdt.dt_turbo_rev.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]


def dji_crc(data):
    # Poly, Initial = 0x00, No reflect, FinalXOR = 0x00
    # 1 added from the left to poly
    compute_crc = mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer
    res = compute_crc(data)
    crc = np.ndarray(3, dtype=np.uint8)
    crc[0] = (res >> 16) & 0xff;
    crc[1] = (res >>  8) & 0xff;
    crc[2] = (res >>  0) & 0xff;
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

def frame(msg):
    if len(msg) != 176:
        return None
    frm = np.zeros(7200, dtype=np.uint8)
    res = libdt.dt_turbo_fwd(frame.ctypes.data_as(ct.POINTER(ct.c_uint8)), msg.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    return frm

def deframe(frame):
    if len(frame) != 7200:
        return None
    msg = np.ndarray(176, dtype=np.uint8)
    res = libdt.dt_turbo_rev(msg.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))

    status = np.int32(np.uint32(res >> 32))
    crc = res & 0xffffffff    
    return msg

def scramble(frame):
    if len(frame) != 7200:
        return None
    frame = frame.reshape((6, len(frame)//6))

    x1_init = np.array([1]+[0,]*30)

    x2_init = np.array([0,0,1,0,0,1,0,0,0,1,1,0,1,0,0,0,1,0,1,0,1,1,0,0,1,1,1,1,0,0,0], dtype=np.uint8)
    x2_init = x2_init[::-1]

    chk = len(x1_init)
    
    M_pn = len(frame)
    Nc = 1600 # As defined in 36.211 7.2

    x1 = np.zeros(Nc + M_pn + len(x1_init), dtype=np.uint8)
    x2 = np.zeros(Nc + M_pn + len(x2_init), dtype=np.uint8)
    res = np.zeros(M_pn, dtype=np.uint8)

    x1[0:chk] = x1_init
    x2[0:chk] = x2_init

    for idx,_ in enumerate(range(M_pn + Nc)):
        x1[chk + idx] = (x1[idx + 2] + x1[idx - 1]) % 2
        x2[chk + idx] = (x2[idx + 2] + x2[idx + 1] + x2[idx] + x2[idx - 1]) % 2

    for idx,_ in enumerate(range(M_pn)):
        res[idx] = (x1[idx + Nc] + x2[idx + Nc]) % 2
    return res


def symbol_mapping(frame):
    return None


# To modulate:
#  1. Encode 91 bytes from drone information              WAIT
#  2. Compute and add CRC                                 DONE
#  3. Turbo encode and rate match                         DONE
#  4. Run scrambler                                       TODO
#  5. Create OFDM symbols                                 TODO
#     a. Generate QPSK symbols                            TODO
#     b. Convert to time domain                           TODO
#     c. Set symbol 4 and 6 to required ZC sequences      DONE
#     d. Add cyclic prefix                                TODO



if __name__ == '__main__':
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
