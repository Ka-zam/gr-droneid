/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dual_trigger_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

dual_trigger::sptr dual_trigger::make(float threshold, int chunk_size)
{
    return gnuradio::make_block_sptr<dual_trigger_impl>(threshold, chunk_size);
}
/*
 * The private constructor
 */
dual_trigger_impl::dual_trigger_impl(float threshold, int chunk_size)
    : gr::sync_block("dual_trigger",
        gr::io_signature::make3(3, 3, sizeof(gr_complex), sizeof(float), sizeof(float)),
        gr::io_signature::make(0, 0, 0)),
        m_port(pmt::mp("pdu"))
{
    m_thr = threshold;
    m_chunk_size = chunk_size;
    message_port_register_out(m_port);

    m_state = WAITING;
    m_total_items = 0;
    m_items_collected = 0;
    m_trig_count = 0;
    
    m_data.resize(m_chunk_size);
    m_t1_samples.resize(3);
    set_output_multiple(1024);
    m_pdu_vector = pmt::make_c32vector(m_chunk_size, gr_complex(0.f,0.f));
}

/*
 * Our virtual destructor.
 */
dual_trigger_impl::~dual_trigger_impl() {
}

void dual_trigger_impl::set_threshold(float t) {
    m_thr = t;
}

void dual_trigger_impl::send_message() {
    pmt::pmt_t meta = pmt::make_dict();
    meta = pmt::dict_add(meta, pmt::mp("type"), pmt::mp("dji droneid"));
    meta = pmt::dict_add(meta, pmt::mp("size"), pmt::mp(m_chunk_size));

    const float n = pwr(m_data.data(), 100);
    const float s = pwr(m_data.data() + 2300, 1080);
    const float snr_db = 20 * std::log10((s - n) / n);

    meta = pmt::dict_add(meta, pmt::mp("snr"), pmt::mp(snr_db));

    // TODO
    m_t1_samples.at(0) = 0.81f;
    m_t1_samples.at(1) = 1.00f;
    m_t1_samples.at(2) = 0.20f;
    float t_frac = toa();
    uint64_t t_int = m_total_items;
    if (t_frac > 1.f) { t_frac -= 1.f; m_total_items += 1; }

    meta = pmt::dict_add(meta, pmt::mp("toa_frac"), pmt::mp(t_frac));
    meta = pmt::dict_add(meta, pmt::mp("toa_int"), pmt::mp(t_int));
    
    for (int i = 0; i < m_chunk_size; ++i) {
        pmt::c32vector_set(m_pdu_vector, i,  m_data.at(i) );
    }
    
    pmt::pmt_t msg = pmt::cons(meta, m_pdu_vector);
    message_port_pub(m_port, msg);
}

float dual_trigger_impl::pwr(const gr_complex* data, const int num) {
    float m;
    float* vec = (float*) volk_malloc(num * sizeof(float), volk_get_alignment());
    volk_32fc_magnitude_squared_32f(vec, data, num);
    volk_32f_accumulator_s32f(&m, vec, num);
    volk_free(vec);
    return std::sqrt(m) / (float) num;
}

float dual_trigger_impl::toa() {
    const float a = .5f * (m_t1_samples.at(0) - m_t1_samples.at(2)) + m_t1_samples.at(1) - m_t1_samples.at(0);
    const float b = m_t1_samples.at(1) - m_t1_samples.at(0) + a;
    return .5 * b / a;
}

int dual_trigger_impl::work(int noutput_items,
                         gr_vector_const_void_star& input_items,
                         gr_vector_void_star& output_items)
{
    auto in = static_cast<const gr_complex*>(input_items[0]);
    auto t1 = static_cast<const float*>(input_items[1]);
    auto t2 = static_cast<const float*>(input_items[2]);

    if (m_state == WAITING) { // Waiting for dual_trigger...
        for (int32_t idx = 0; idx < noutput_items; ++idx) {
            const bool dual_trigger1 = (*t1 > m_thr);
            const bool dual_trigger2 = (*t2 > m_thr);

            if (dual_trigger1 && dual_trigger2) {
                m_state = TRIGGERED;
                m_trig_count++;
                // TODO
                // Save TOA samples
                // Collect
                int items_to_collect = std::min(noutput_items - idx - 1, m_chunk_size);
                for (int i = 0; i < items_to_collect; ++i) {
                    m_data.at(i) = *in++;
                }
                m_items_collected += items_to_collect;
                int rem = m_chunk_size - m_items_collected;
                if (!rem) { 
                    send_message();
                    m_items_collected = 0;
                    m_state = WAITING;
                    m_total_items += m_chunk_size;
                    return m_chunk_size;                    
                }
                m_total_items += noutput_items;
                return noutput_items;
            }
            in++;
            t1++;
            t2++;
        }
        // I got nothing, drop everything and keep listening....
        m_t1_last_sample = *(t1 - 1); // decr ptr
        m_total_items += noutput_items;
        return noutput_items;
    }
    else if (m_state == TRIGGERED){
        int rem = m_chunk_size - m_items_collected;
        if (!rem) {
            send_message();
            m_items_collected = 0;
            m_state = WAITING;
            return 0;
        }
        // Still collecting...
        int items_to_collect = std::min(noutput_items, rem);
        for (int i = 0; i < items_to_collect; ++i) {
            m_data.at(i + m_items_collected) = *in++;
        }
        m_items_collected += items_to_collect;
        return items_to_collect;
    }
    else {
        // What are we even doing here...
        return noutput_items;
    }
}

} /* namespace droneid */
} /* namespace gr */
