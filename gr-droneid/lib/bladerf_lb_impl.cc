/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bladerf_lb_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

bladerf_lb::sptr bladerf_lb::make(int samp_rate)
{
    return gnuradio::make_block_sptr<bladerf_lb_impl>(samp_rate);
}


/*
 * The private constructor
 */
bladerf_lb_impl::bladerf_lb_impl(int samp_rate)
    : gr::sync_block("bladerf_lb",
    gr::io_signature::make(0,0,0), gr::io_signature::make(1,1,sizeof(int32_t))),
    m_port(pmt::mp("error")),
    m_samp_rate(samp_rate),
    m_failures(0),
    m_count(0)
{
    message_port_register_out(m_port);
    set_max_noutput_items(SAMPLES_PER_BUFFER);
    m_uint32buf = (uint32_t*) volk_malloc(SAMPLES_PER_BUFFER * sizeof(uint32_t), volk_get_alignment());
   /*
      bladeRF setup
    */
    bladerf_set_usb_reset_on_open(true);

    //bladerf_frequency fc = 1000000000;
    //bladerf_bandwidth bw = 56000000;
    //bladerf_gain_mode BLADERF_GAIN_MGC;

    bladerf_channel ch = BLADERF_CHANNEL_RX(0);
    int status = bladerf_open(&m_dev, "*:instance=0");
    if (status != 0) {
      fprintf(stderr, "Unable to open device: %s\n", bladerf_strerror(status));
      exit(0);
    }

    bladerf_sample_rate _actual;
    status = bladerf_set_sample_rate(m_dev, ch, m_samp_rate, &_actual);
    if (status!=0) {
      fprintf(stderr,
              "Problem while setting sampling rate: %s\n",
              bladerf_strerror(status));
    } else { std::cout << "Set samp_rate to " << _actual << "\n";}

    status = bladerf_set_bias_tee(m_dev, ch, false);
    status = bladerf_set_gain(m_dev, ch, 0);
    status = bladerf_set_gain_mode(m_dev, ch, BLADERF_GAIN_MGC);
    status = bladerf_set_frequency(m_dev, ch, 1000000000);


    status = bladerf_sync_config(m_dev,                   // device
                               BLADERF_RX_X1,           // channel layout
                               BLADERF_FORMAT_SC16_Q11, // format
                               NUMBER_OF_BUFFERS,       // # buffers
                               SAMPLES_PER_BUFFER,      // buffer size
                               NUMBER_OF_TRANSFERS,     // # transfers
                               STREAM_TIMEOUT_MS);      // timeout (ms)
    if (status<0) {
      fprintf(stderr, "Couldn't configure RX streams: %s\n", bladerf_strerror(status));
    }

    /* Enable RX 0 */
    status = bladerf_enable_module(m_dev, ch, true);
    if (status<0) {
      fprintf(stderr, "Couldn't enable RX module: %s\n", bladerf_strerror(status));
      exit(-1);
    }

    status = bladerf_set_rx_mux(m_dev, BLADERF_RX_MUX_32BIT_COUNTER);
    if (status<0) {
      fprintf(stderr, "Couldn't set RX MUX mode: %s\n", bladerf_strerror(status));
      exit(-1);
    }
}

/*
 * Our virtual destructor.
 */
bladerf_lb_impl::~bladerf_lb_impl() {
    volk_free(m_uint32buf);
}

int bladerf_lb_impl::work(int noutput_items,
    gr_vector_const_void_star& input_items,
    gr_vector_void_star& output_items)
{
    int status = bladerf_sync_rx(m_dev, (void*) m_uint32buf, noutput_items, NULL, STREAM_TIMEOUT_MS);
    if (status) {
        fprintf(stderr, "%s: %s\n", "bladeRF stream error", bladerf_strerror(status));
        m_failures++;
        if (m_failures >= MAX_CONSECUTIVE_FAILURES) {
            fprintf(stderr, "%s\n", "Consecutive error limit hit. Shutting down.");
            return WORK_DONE;
        }
    } else {
        m_failures = 0;
    }

    m_count += noutput_items;

    int32_t diff = m_uint32buf[noutput_items - 1] - m_uint32buf[0];
    if ( diff != noutput_items-1) {
        pmt::pmt_t msg = pmt::intern("Error: " + std::to_string(diff));
        message_port_pub(m_port, msg);
    }

    /*
    if (m_count > 15000000) { 
        m_count = 0;
        std::string s = "diff: " + std::to_string(diff);
        s += std::to_string(*m_uint32buf) + "\n";
        //s += std::to_string(*(m_uint32buf + 100)) + "\n";
        pmt::pmt_t msg = pmt::intern(s);
        message_port_pub(m_port, msg);
        std::cout << "m    : " << m_uint32buf[0] << "\n";
        std::cout << "m   1: " << m_uint32buf[1] << "\n";
        std::cout << "m noi: " << m_uint32buf[noutput_items-1] << "\n";
    }

    */

    /*

    m_count++;
    if (m_count % 1000) {
        std::cout << "uint32_t: " << *m_uint32buf << "  +100: " << *(m_uint32buf + 100) << "\n";
        pmt::pmt_t msg = pmt::intern("Got called with noutput_items: " + std::to_string(noutput_items));
        message_port_pub(m_port, msg);
        m_count = 0;
    }
    */

    auto out = static_cast<uint32_t*>(output_items[0]);
    memcpy(out, m_uint32buf, noutput_items * sizeof(uint32_t));
    return noutput_items;
}

} /* namespace droneid */
} /* namespace gr */
