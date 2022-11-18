import numpy as np
from struct import pack
import datetime
now = datetime.datetime.now

def rs_makefile(bb, samp_rate, period_ms=10):
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

def rs_wv(iq_data, samp_rate=15.36e6, comment="DroneID message"):
    iq_data /= max( [np.max(abs(np.real(iq_data))), np.max(abs(np.imag(iq_data)))] )
    scale = 32767
    iq_data *= scale
    num_bytes = len(iq_data) * 2 * 2 + 1 # le_int16 encoding
    date_str = now().strftime('%Y-%m-%d;%H:%M:%S')

    bs  = b'{TYPE: SMU-WV,0}'
    bs += b'{CLOCK: ' + "{:9.3f}".format(samp_rate).encode('ascii') + b'}'
    bs += b'{LEVEL OFFS: 3.010300,0.000000}'
    bs += b'{DATE: ' + "{}".format(date_str).encode('ascii') + b'}'
    bs += b'{COPYRIGHT: Skysense AB}'
    bs += b'{COMMENT:' + " {}".format(comment).encode('ascii') + b'}'   
    bs += b'{SAMPLES: ' + "{:d}".format(len(iq_data)).encode('ascii') + b'}'

    bs += b'{WAVEFORM-' + "{:d}".format(num_bytes).encode('ascii') + b':#'
    for z in iq_data:
        bs += pack('<h', round(np.real(z)))
        bs += pack('<h', round(np.imag(z)))
    bs += b'}'
    return bs

def rs_blank(num_samples, samp_rate=15.36e6):
    num_samples = int(num_samples)
    num_bytes = num_samples * 2 * 2 + 1 # le_int16 encoding
    date_str = now().strftime('%Y-%m-%d;%H:%M:%S')

    bs  = b'{TYPE: SMU-WV,0}'
    bs += b'{CLOCK: ' + "{:9.3f}".format(samp_rate).encode('ascii') + b'}'
    bs += b'{LEVEL OFFS: 200,200}'
    bs += b'{DATE: ' + "{}".format(date_str).encode('ascii') + b'}'
    bs += b'{COPYRIGHT: Skysense AB}'
    bs += b'{COMMENT: Blank segment}'
    bs += b'{SAMPLES: ' + "{:d}".format(num_samples).encode('ascii') + b'}'

    bs += b'{WAVEFORM-' + "{:d}".format(num_bytes).encode('ascii') + b':#'
    for _ in range(num_samples):
        bs += pack('<h', 0)
        bs += pack('<h', 0)
    bs += b'}'
    return bs
