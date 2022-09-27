import numpy as np

droneid = {}
droneid["carrier_spacing"]  = 15.0e3 # Hz
droneid["data_carriers"]    = 600
droneid["zc_root_symbol_4"] = 600
droneid["zc_root_symbol_6"] = 147
droneid["cp_seq"]           = [1, 0, 0, 0, 0, 0, 0, 0, 0, 1] # 1 is long

def fft_size(samp_rate):
    return int(np.round(samp_rate / droneid["carrier_spacing"]))

def short_cp(samp_rate):
    return int(np.round(samp_rate * 0.0000046875))

def long_cp(samp_rate):
    return int(np.round(samp_rate / 192000.0))

def left_gc(samp_rate):
    n = fft_size(samp_rate)
    gc = n - droneid["data_carriers"]
    return gc // 2

def data_carrier_indices(samp_rate):
    n = fft_size(samp_rate)
    idx_dc = n // 2
    indices = np.zeros(n)
    #indices[]

    return 1

def demodulate(x, samp_rate):
    return 1
    #

def zc2(sr):
    root = droneid["zc_root_symbol_4"]

    n = fft_size(sr)
    guard_carriers = n - droneid["data_carriers"]
    ng = guard_carriers // 2

    b = np.exp(-1j * np.pi * root * np.array([x * (x + 1.) for x in range(600)]) / 601 )
    b[n // 2] = 0.0
    a = np.zeros(ng)
    zc_w = np.concatenate([a, b, a])
    zc_w = np.fft.fftshift(zc_w)
    zc = np.fft.ifft(zc_w)
    return np.conj(zc)

def create_zc_sequence(samp_rate, symbol=4):
    # TODO: This doesn't seem to work for 30.72Msps
    # DJI OFDM settings
    if samp_rate < 15.36e6:
        return None

    if symbol==4:
        root = droneid["zc_root_symbol_4"]
    elif symbol==6:
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
    return np.conj(zc)

def trigger(fname, trsh=1.0, samp_rate=15.36e6):
    fid = open(fname,'rb')
    zc = create_zc_sequence(samp_rate)
    chunk = len(zc) * 2 * 4 # IQ in fc32
    file_len = fid.seek(0,2)
    fid.seek(0)
    print("File length: {:5.2f} GB".format(file_len / 1024.**3))
    ofs = 0
    while (ofs < file_len - chunk):
        data = np.frombuffer( fid.read(chunk) , dtype=np.complex64)
        data = np.fft.ifft( zc * np.fft.fft(data) )
        idx = np.where( np.abs(data) > trsh )[0]
        if len(idx) > 0:
            for x in idx:
                print("idx: {:8d}\n".format(x + ofs // 8))
        ofs += chunk

def feng_zc(q, bandwidth, sample_rate, is_FPV=False):
    """
    Function is used to generate ZC sequence for drones except FPV
    For PSS sequence, always set q to 1
    :param q: int type
            Root number of ZC sequence, ty
    :param bandwidth: {10,20}, optional
            The channel bandwidth of drones
    :param sample_rate: {15.36, 30.72, 61.44}, optional
            The sample rate, 15.36 means under 15.36M sample rate
    :return: ndarray
            The ZC sequence in time domain
    """
    pi = np.pi
    ifft = np.fft.ifft
    fft = np.fft.fft
    fftshift = np.fft.fftshift
    if bandwidth == 10:
        # Under 10M channel, the number of subcarriers used is 601 so set the length of ZC sequence to 601
        if not is_FPV:
            Nzc = 601
            # The number of left unused subcarriers is 212 and 211 for right
            # Add zero on these subcarriers
            ZC_local = [0 for i in range(212)] + [np.exp(-1j * q * pi * n * (n + 1) / Nzc) for n in range(Nzc)] + \
                       [0 for i in range(211)]
        else:
            Nzc = 307
            ZC_even = np.array([np.exp(-1j * q * pi * ((n) * (n + 1)) / Nzc) for n in range(0, Nzc)])[:301]
            ZC_local = np.zeros(601, dtype=np.complex64)
            ZC_local[::2] = ZC_even
            ZC_local = [0 for i in range(212)] + list(ZC_local) + \
                       [0 for i in range(211)]
            ZC_local[512] = 0
        # Do the IFFT switch to time domain sequence
        Zc_sequence= list(ifft(fftshift(ZC_local)))
        if sample_rate == 15.36:
            # Under 15.36M sample rate, the sequence generated is the sequence we sample
            return Zc_sequence
        elif sample_rate == 30.72:
            # Under 30.72M sample rate, each element need to appear twice
            new_ZC_sequence = np.zeros(2048,dtype=np.complex64)
            # The even index sequence of new sequence is ZC sequence
            # The odd index sequence of new sequence is ZC sequence
            # Then each element repeated
            new_ZC_sequence[::2] = Zc_sequence
            new_ZC_sequence[1::2] = Zc_sequence
            return new_ZC_sequence
        elif sample_rate == 61.44:
            # Under 61.44MM sample rate, each element need to appear four times
            new_ZC_sequence = np.zeros(4096,dtype=np.complex64)
            new_ZC_sequence[::4] = Zc_sequence
            new_ZC_sequence[1::4] = Zc_sequence
            new_ZC_sequence[2::4] = Zc_sequence
            new_ZC_sequence[3::4] = Zc_sequence
            return new_ZC_sequence
        else:
            print("Sample rate should be 15.36M, 30.72M or 61.44M")
            return
    elif bandwidth == 20:
        if not is_FPV:
            # Under 20M channel, the number of subcarriers used is 1201 so set the length of ZC sequence to 1201
            Nzc = 1201
            # The number of left unused subcarriers is 424 and 423 for right
            # Add zero on these subcarriers
            ZC_local = [0 for i in range(424)] + [np.exp(-1j * q * pi * n * (n + 1) / Nzc) for n in range(Nzc)] + \
                       [0 for i in range(423)]
        else:
            Nzc = 601
            ZC_even = np.array([np.exp(-1j * q * pi * ((n) * (n + 1)) / Nzc) for n in range(0, Nzc)])[:601]
            ZC_local = np.zeros(1201, dtype=np.complex64)
            ZC_local[::2] = ZC_even
            ZC_local = [0 for i in range(424)] + list(ZC_local) + \
                       [0 for i in range(423)]
            ZC_local[1024] = 0
        Zc_sequence = list(ifft(fftshift(ZC_local)))
        if sample_rate == 15.36:
            # The sequence generated is under 30.72M sample rate
            # Therefore under 15.36 sample rate, only half of the sequence are collected
            print("sample rate must over 15.36")
            return
        elif sample_rate == 30.72:
            return Zc_sequence
        elif sample_rate == 61.44:
            new_ZC_sequence = np.zeros(4096,dtype=np.complex64)
            new_ZC_sequence[::2] = Zc_sequence
            new_ZC_sequence[1::2] = Zc_sequence
            return new_ZC_sequence
    elif bandwidth == 40:
        if not is_FPV:
            Nzc = 2401
            # The number of left unused subcarriers is 424 and 423 for right
            # Add zero on these subcarriers
            ZC_local = [0 for i in range(848)] + [np.exp(-1j * q * pi * n * (n + 1) / Nzc) for n in range(Nzc)] + \
                       [0 for i in range(847)]
        else:
            Nzc = 1201
            ZC_even = np.array([np.exp(-1j * q * pi * ((n) * (n + 1)) / Nzc) for n in range(0, Nzc)])[:601]
            ZC_local = np.zeros(1201, dtype=np.complex64)
            ZC_local[::2] = ZC_even
            ZC_local = [0 for i in range(848)] + list(ZC_local) + \
                       [0 for i in range(847)]
            ZC_local[2048] = 0
        Zc_sequence = list(ifft(fftshift(ZC_local)))
        if sample_rate == 15.36 or sample_rate == 30.72:
            # The sequence generated is under 30.72M sample rate
            # Therefore under 15.36 sample rate, only half of the sequence are collected
            print("sample rate must over 30.72")
            return
        elif sample_rate == 61.44:
            return Zc_sequence
    else:
        print("Channel bandwidth should be 10M, 20M or 40M")
        return
