#!/usr/bin/python3
import numpy as np

class e4400:
    """Commands for E443X signal generators"""
    def __init__(self):
        pass

    def make_files(self, bb, samp_rate=15.36e6, period_ms=10):
        # Make the first file an even period_ms in length
        if period_ms < 2:
            print("Minimum period is 2ms")
            return None
        elif period_ms > 640:
            print("Maximum period is 640ms")
            return None
        samples_per_period = round(samp_rate * period_ms * 1e-3)
        samples_per_ms = round(samp_rate * 1 * 1e-3)
        prf = samp_rate / samples_per_period

        # round up to even ms
        pad_len = samples_per_ms - len(bb)
        cmd_bb = self.cmd_arb_iq(np.real(bb), np.imag(bb), "droneid_msg")
        cmd_zero_pad = self.cmd_arb_iq(np.zeros(pad_len), None, "silence_pad")
        cmd_zero_ms = self.cmd_arb_iq(np.zeros(samples_per_ms), None, "silence_one_ms")
        return [cmd_bb, cmd_zero_pad, cmd_zero_ms]

    def cmd_sequence(self, name="pfr_seq", seq_list=[[]]):
        if seq_list[0] == [] or len(seq_list[0]) != 4:
            return None
        cmd  = ':source:radio:arb:seq "seq:'
        cmd += name + '",'
        for seg in seq_list:
            cmd += '"arbi:' + seg[0] + '"'
            cmd += ',' + str(seg[1])
            cmd += ',' + str(seg[2])
            cmd += ',' + str(seg[3]) + ','
        return cmd

    def cmd_select_waveform(self, file_name,file_type="SEQ"):
        return ":source:radio:arb:waveform \"{}:{}\"".format(file_type, file_name)

    def cmd_samp_rate(self, samp_rate=15.36e6):
        return ":source:radio:arb:clock:srate {:9.3f} Hz".format(samp_rate)

    def cmd_filter_through(self):
        return ":source:radio:arb:rfilter through"

    def cmd_mem_cat(self):
        return "mmem:cat? \"arbi:\""

    def cmd_arb(self, state=False):
        s = "on" if state else "off"
        return ":source:radio:arb:state {}".format(s)

    def cmd_mod(self, state=False):
        s = "on" if state else "off"
        return ":output:modulation:state {}".format(s)

    def cmd_power(self, power_dbm=-50.0):
        return "pow:ampl {:9.3f} dBm".format(power_dbm)

    def cmd_freq(self, freq_mhz=2450):
        return "freq {:9.3f} MHz".format(freq_mhz)

    def cmd_alc(self, state=False):
        s = "on" if state else "off"
        return "pow:alc {}".format(s)

    def cmd_output(self, state=False):
        s = "on" if state else "off"
        return ":output:state {}".format(s)

    def cmd_copy(self, file_name, direction="arbi"):
        if direction == "arbi":
            (copy_to, copy_from) = ("arbi", "nvarbi")
        elif direction == "arbq":
            (copy_to, copy_from) = ("arbq", "nvarbq")
        elif direction == "nvarbi":
            (copy_to, copy_from) = ("nvarbi", "arbi")
        else:
            raise ValueError("direction = {} is not supported".format(direction))
        return ":memory:copy:name \"{0}:{2}\",\"{1}:{2}\"".format(copy_from, copy_to, file_name)

    def encode(self, float_array, flags_array=[]):
        if len(float_array) < 16:
            return None
        # Build bytesarray
        no_of_bytes = 2 * len(float_array)
        extra_bytes = 1 + 1 + len(str(no_of_bytes)) + 1
        #            '#'  no_of_digits   size_str     \n
        bytes_array = np.ndarray( no_of_bytes + extra_bytes, dtype=np.uint8 )
        bytes_array.fill(0xaa)
        float_array = np.clip(float_array, -1.0, 1.0)
        
        bytes_array[0] = ord("#")
        bytes_array[1] = ord(str(len(str(no_of_bytes))))
        for idx, char in enumerate(str(no_of_bytes)):
            bytes_array[2 + idx] = ord(char)

        idx = extra_bytes - 1
        for val in float_array:
            scale = 8191 if val >= 0.0 else 8192
            number = np.uint16(round(val * scale))
            number += 8192
            msb = number >> 8
            lsb = number & 0x00ff
            bytes_array[idx] = msb
            bytes_array[idx + 1] = lsb
            idx += 2

        bytes_array[-1] = ord(b'\n')
        return bytes(bytes_array)

    def decode(self, raw_bytes):
        # Decode the raw string of bytes from an E4430B ARB memory
        # Format:
        # b'#41500 \x00\x1f\xac.....'
        #   ^ Start symbol
        #    ^ Number of decimal digits in the following number
        #      ^ Length in bytes
        #         ^ Space character
        #          ^ Little endian uint16_t to be interpreted 
        # 
        if raw_bytes[:1] != b'#':
            return None
        digits = int(raw_bytes[1:2])
        size = int(raw_bytes[2:digits + 2])
        raw_bytes = raw_bytes[digits + 2:-1:] # Ditch \n at end
        if size != len(raw_bytes):
            return None
        values = np.ndarray(size // 2, dtype=np.float32)
        flags = np.ndarray(size // 2, dtype=np.int8)
        i = 0
        for k in range(len(values)):
            integer = ((raw_bytes[i] & 0b00111111) << 8) | ( raw_bytes[i + 1])
            integer -= 8192
            scale = 8191.0 if integer >= 0 else 8192.0
            values[k] = integer / scale
            flags[k] = raw_bytes[i] >> 6
            i += 2
        return (values, flags)

    def cmd_arb_iq(self, I_array, Q_array=None, wvf_name='waveform'):
        if len(I_array) < 16:
            return None    
        cmd_i = ":MMEM:DATA \"ARBI:{:s}\", ".format(wvf_name[:16]).encode(encoding='ascii')
        cmd_i += self.encode(I_array)
        if Q_array is not None:
            cmd_q = ":MMEM:DATA \"ARBQ:{:s}\", ".format(wvf_name[:16]).encode(encoding='ascii')
            cmd_q += self.encode(Q_array)
        else:
            cmd_q = None
        return (cmd_i, cmd_q)

    def cmd_nvarb_iq(self, I_array, Q_array=None, wvf_name='waveform'):
        if len(I_array) < 16:
            return None    
        cmd_i = ":MMEM:DATA \"NVARBI:{:s}\", ".format(wvf_name[:16]).encode(encoding='ascii')
        cmd_i += self.encode(I_array)
        if Q_array is not None:
            cmd_q = ":MMEM:DATA \"NVARBQ:{:s}\", ".format(wvf_name[:16]).encode(encoding='ascii')
            cmd_q += self.encode(Q_array)
        else:
            cmd_q = None
        return (cmd_i, cmd_q)