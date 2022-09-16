#!/usr/bin/python3
import numpy as np
import ctypes as ct
from random import sample
from crcmod import mkCrcFun

# Poly, Initial = 0x00, No reflect, FinalXOR = 0x00
# 1 added from the left to poly
compute_crc = mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer

libdt = ct.cdll.LoadLibrary("libdt.so")
libdt.dt_turbo_fwd.restype = ct.c_uint32
libdt.dt_turbo_fwd.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
libdt.dt_turbo_rev.restype = ct.c_uint64
libdt.dt_turbo_rev.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]


def dji_crc(data):
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
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  76, 201], dtype=np.uint8)
    else:
        msg = np.ndarray(91, dtype=np.uint8)
        msg.fill(0xff)   

    scrap = np.ndarray(82, dtype=np.uint8)
    scrap.fill(0x00)

    msg = np.concatenate([msg, scrap])
    crc = dji_crc(msg)
    return np.concatenate([msg, crc])

def frame(msg):
    if len(msg) != 176:
        return None
    out = np.zeros(7200, dtype=np.uint8)
    res = libdt.dt_turbo_fwd(out.ctypes.data_as(ct.POINTER(ct.c_uint8)), msg.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    return out

def deframe(frame):
    if len(frame) != 7200:
        return None
    msg = np.ndarray(176, dtype=np.uint8)
    res = libdt.dt_turbo_rev(msg.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))









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
    status = np.int32(np.uint32(res >> 32))
    crc = res & 0xffffffff

    decoded_frame.tofile("frame.decoded.bin")
    print("Decoder CRC: {:3d}   Status: {:02x}".format(status, crc))
