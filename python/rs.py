import numpy as np
from struct import pack
import datetime
now = datetime.datetime.now

def makefile(bb, samp_rate, period_ms=10, sro_ppm=0):
    # Make the first file an even period_ms in length
    if period_ms < 1:
        print("Minimum period is 1ms")
        return None
    elif period_ms > 1000:
        print("Maximum period is 640ms")
        return None
    samples_per_period = round(samp_rate * period_ms * 1e-3)
    prf = samp_rate / samples_per_period

    # Generate droneid message from bb and fill up to one period
    blank_len = samples_per_period - len(bb)
    bb = np.concatenate([bb, np.zeros(blank_len, dtype=np.complex64)])
    bs = rs_wv(bb, samp_rate, "DroneID, PRF: {:6.2f} Hz".format(prf))
    return bs

def wv(iq_data, samp_rate=15.36e6, comment="DroneID message",sro_ppm=0):
    iq_data /= max( [np.max(abs(np.real(iq_data))), np.max(abs(np.imag(iq_data)))] )
    scale = 32767
    iq_data *= scale
    num_bytes = len(iq_data) * 2 * 2 + 1 # le_int16 encoding
    date_str = now().strftime('%Y-%m-%d;%H:%M:%S')
    actual_samp_rate = samp_rate * (1. + sro_ppm * 1e-6)
    iq_bytes = b''
    for z in iq_data:
        iq_bytes += pack('<h', round(np.real(z)))
        iq_bytes += pack('<h', round(np.imag(z)))

    bs  = b'{CLOCK: ' + "{:9.3f}}}".format(actual_samp_rate).encode('ascii')
    bs += b'{LEVEL OFFS: 3.010300,0.000000}'
    bs += b'{DATE: ' + "{}}}".format(date_str).encode('ascii')
    bs += b'{COPYRIGHT: Skysense AB}'
    bs += b'{COMMENT:' + " {}}}".format(comment).encode('ascii')
    bs += b'{SAMPLES: ' + "{:d}}}".format(len(iq_data)).encode('ascii')
    bs += b'{WAVEFORM-' + "{:d}:#".format(num_bytes).encode('ascii') + iq_bytes + b'}'
    bs  = b'{TYPE: SMU-WV,' + "{:d}}}".format(crc(iq_bytes)).encode('ascii') + bs
    return bs

def blank(num_samples, samp_rate=15.36e6, sro_ppm=0):
    num_samples = int(num_samples)
    num_bytes = num_samples * 2 * 2 + 1 # le_int16 encoding
    date_str = now().strftime('%Y-%m-%d;%H:%M:%S')
    actual_samp_rate = samp_rate * (1. + sro_ppm * 1e-6)    

    bs += b'{CLOCK: ' + "{:9.3f}}}".format(actual_samp_rate).encode('ascii')
    bs += b'{LEVEL OFFS: 200,200}'
    bs += b'{DATE: ' + "{}}}".format(date_str).encode('ascii')
    bs += b'{COPYRIGHT: Skysense AB}'
    bs += b'{COMMENT: Blank segment}'
    bs += b'{SAMPLES: ' + "{:d}}}".format(num_samples).encode('ascii')
    bs += b'{WAVEFORM-' + "{:d}:#".format(num_bytes).encode('ascii')
    bs += b'\x00' * num_samples * 2 * 2 + b'}'
    bs  = b'{TYPE: SMU-WV,' + "{:d}}}".format(crc(iq_bytes)).encode('ascii') + bs    
    return bs

def cfo_sco(crystal_err_ppm=0., fc=2414.5e6, samp_rate=15.36e6):
    scale = 1. + (crystal_err_ppm * 1e-6)
    return (fc * scale, samp_rate * scale)

def set_clock(samp_rate=15.36e6):
    return "bb:arb:clock {:9.6f}".format(samp_rate)

def set_frequency(freq=2.415e9):
    return "frequency {:9.6f}".format(freq)

def set_power(pwr=-60.0):
    return "power {:9.6f}".format(pwr)

def set_output(state=False):
    return "output {:1d}".format(1 if state else 0)

def set_file(filename):
    # for SMCV100B you can transfer file via FTP, user/pass: instrument/instrument
    # place file in /user
    # 'mmemory:cat?' reports file from this directory
    # they appear under /var/user/
    # give filename without .wv extension
    filename = "/var/user/" + filename
    return "bb:arb:wav:sel \"{}\"".format(filename)

def crc(data):
    if len(data) % 4 != 0:
        return None
    num_samples = len(data) // 4
    res = 0xa50f74ff
    ptr = 0
    for _ in range(num_samples):
        res ^= int.from_bytes(data[ptr:ptr + 4], byteorder='little', signed=False)
        ptr += 4
    return res
