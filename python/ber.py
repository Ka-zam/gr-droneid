#!/usr/bin/python3
import numpy as np
from sys import argv, exit
import djidecoder
import djiencoder

def ber(tx, rx):
    err = 0
    for t,r in zip(tx["scrambled_bits"], rx["raw_bits"]):
        if r != t:
            err += 1
    return err

if __name__ == '__main__':
    #import matplotlib.pyplot as plt
    if len(argv) == 1:
        print("Usage:\n {} SNR_DB".format(argv[0]))
        exit(0)
    try:
        snr_db = float(argv[1])
    except Exception as e:
        raise e
    
    e = djiencoder.djiencoder()
    d = djidecoder.djidecoder()

    tx = e.encode(snr_db)
    rx = d.decode(tx["signal"])
    rx["descrambled_bits"] = d.descramble(rx["raw_bits"])
    print("Serial    : {}".format(rx["payload"][7: 7 + 16]))
    print("Raw errors: {}".format(ber(tx,rx)))
