import numpy as np

def get_fft_size(samp_rate):
    CARRIER_SPACING = 15.0e3 #Hz    
    return np.int(np.round(samp_rate / CARRIER_SPACING))


def create_zc_sequence(samp_rate, root=600.0):
    CARRIER_SPACING = 15.0e3 #Hz
    FFT_SIZE = np.int(np.round(samp_rate / CARRIER_SPACING))
    LONG_CP_SIZE = np.round(samp_rate / 192000.0)
    SHRT_CP_SIZE = np.round(samp_rate * 0.0000046875)
    OCCUPIED_CARRIERS_INC_DC = 601
    OCCUPIED_CARRIERS_EXC_DC = OCCUPIED_CARRIERS_INC_DC - 1
    GUARD_CARRIERS = FFT_SIZE - OCCUPIED_CARRIERS_EXC_DC
    LEFT_GUARD_CARRIERS = GUARD_CARRIERS // 2

    inp = np.zeros(FFT_SIZE, dtype=np.complex128)

    for idx,_ in enumerate(range(OCCUPIED_CARRIERS_INC_DC)):
        x = np.pi * root * idx * (idx + 1.0) / OCCUPIED_CARRIERS_INC_DC
        inp[idx + LEFT_GUARD_CARRIERS] = np.exp(-1j * x)
    inp[FFT_SIZE // 2]  = 0.0
    inp = np.fft.fftshift(inp)
    zc = np.conj(np.fft.ifft(inp))
    return zc
