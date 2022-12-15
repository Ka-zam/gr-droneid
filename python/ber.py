#!/usr/bin/python3
import numpy as np
from sys import argv, exit
import djidecoder
import djiencoder


if __name__ == '__main__':
    #import matplotlib.pyplot as plt
    if len(argv) == 1:
        print("Usage:\n {} SNR_DB".format(argv[0]))
        exit(0)
    try:
        snr = float(argv[1])
    except Exception as e:
        raise e
    
    e = djiencoder.djiencoder()
    d = djidecoder.djidecoder()

    tx = e.msg_to_signal(snr)
    rx = d.decode(tx)
    print("Serial: {}".format(rx["payload"][7: 7 + 16]))
