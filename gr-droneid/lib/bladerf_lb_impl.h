/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_BLADERF_LB_IMPL_H
#define INCLUDED_DRONEID_BLADERF_LB_IMPL_H

#include <gnuradio/droneid/bladerf_lb.h>
#include <libbladeRF.h>
#include <volk/volk.h>

namespace gr {
namespace droneid {

class bladerf_lb_impl : public bladerf_lb
{
private:
    const pmt::pmt_t m_port;
    struct bladerf* m_dev;
    int m_samp_rate;
    uint32_t* m_uint32buf;
    int m_failures;
    int m_count;

    /* Scaling factor used when converting from int16_t to float */
    static constexpr float SCALING_FACTOR = 2048.0f;
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
    static constexpr int SAMPLES_PER_BUFFER = 4096;
    static constexpr int NUMBER_OF_BUFFERS = 512;
    static constexpr int NUMBER_OF_TRANSFERS = 32;
    static constexpr int STREAM_TIMEOUT_MS = 3000;
    static constexpr int EXT_CLOCK_DWELL_US = 2000;    

public:
    bladerf_lb_impl(int samp_rate);
    ~bladerf_lb_impl();
    int work(int noutput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_BLADERF_LB_IMPL_H */
