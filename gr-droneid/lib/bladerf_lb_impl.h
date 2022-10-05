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
    gr_complex* m_complex32buf;
    int m_failures;
    uint64_t m_count;

    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
    static constexpr int SAMPLES_PER_BUFFER = 1024*32;
    static constexpr int NUMBER_OF_BUFFERS = 1024;
    static constexpr int NUMBER_OF_TRANSFERS = 32;
    static constexpr int STREAM_TIMEOUT_MS = 6000;
    static constexpr int SYNC_TIMEOUT_MS = 9000;
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
