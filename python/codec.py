#!/usr/bin/python3
import numpy as np
import ctypes as ct
# Poly, Initial = 0x00, No reflect, FinalXOR = 0x00
# 1 added from the left to poly
# crc = crcmod.mkCrcFun(0x1864cfb, 0x00, False, 0x00) # returns integer
def encode(msg_dict):
    DJI_MSG_BYTES = 91
    DJI_SCRAP_BYTES = 82
    MSG_BYTES = DJI_MSG_BYTES + DJI_SCRAP_BYTES
    payload = bytearray(MSG_BYTES) # 

if __name__ == '__main__':
    frame = np.array([ 88,  16,   2,   0,   0,   0,   0,  48,  49,  50,  51,  52,  53,\
        54,  55,  56,  57,  97,  98,  99, 100,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,  19,   0,   0,   0,   0,   0,   0,   0,   0,   0,\
         0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  76, 201], dtype=np.uint8)
    out = np.zeros(7200, dtype=np.uint8)
    libdt = ct.cdll.LoadLibrary("libdt.so")
    libdt.dt_turbo_fwd.restype = ct.c_uint32
    libdt.dt_turbo_fwd.argtypes = [ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint8)]
    res = libdt.dt_turbo_fwd(out.ctypes.data_as(ct.POINTER(ct.c_uint8)), frame.ctypes.data_as(ct.POINTER(ct.c_uint8)))
    out.tofile("frame.codec.bin")

    print("Computed CRC for known frame: {:02x}".format(res))
