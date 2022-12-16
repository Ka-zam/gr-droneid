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

def fer(tx, rx):
    err = 0
    for t,r in zip(tx["payload"], rx["payload"]):
        err += (t ^ r).bit_count()
    return err

if __name__ == '__main__':
    import matplotlib.pyplot as plt
    if len(argv) == 1:
        print("Usage:\n {} SNR_DB".format(argv[0]))
        exit(0)
    try:
        snr_db = float(argv[1])
        sto = float(argv[2])
    except Exception as e:
        raise e
    
    enc = djiencoder.djiencoder()
    dec = djidecoder.djidecoder()

    tx = enc.encode(snr_db, sto=sto)
    #plt.plot(np.real(tx["signal"]))
    #plt.show()
    rx = dec.decode(tx)
    print("Sig        : {}".format(len(rx["signal"])))
    print("Start S4,S6: {}".format(rx["start_idx"]))
    print("Serial     : {}".format(rx["payload"][7: 7 + 16]))
    print("BER        : {}".format(ber(tx,rx)))
    print("FER        : {}".format(fer(tx,rx)))

    fig = plt.figure(figsize=(10, 10))
    plt.subplot(3,3,1)
    plt.subplots_adjust(left=.05, bottom=0.05, right=.95, top=.95)
    for r in range(rx["constellation"].shape[0]):
        s = rx["constellation"][r,:]
        plt.subplot(3,3,r+1)
        plt.plot(np.real(s), np.imag(s), '*')
    plt.draw()
    plt.waitforbuttonpress(0)
    plt.close(fig)    
