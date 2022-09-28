/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trigger_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

trigger::sptr trigger::make(float threshold, int chunk_size)
{
    return gnuradio::make_block_sptr<trigger_impl>(threshold, chunk_size);
}
/*
 * The private constructor
 */
trigger_impl::trigger_impl(float threshold, int chunk_size)
    : gr::sync_block("trigger",
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
    set_output_multiple(1024);

    /*
    m_pdu_meta = pmt::PMT_NIL;
    pmt::pmt_t m_pdu_meta = pmt::make_dict();
    m_pdu_meta = pmt::dict_add(m_pdu_meta, pmt::intern("type"), pmt::intern("iq"));
    m_pdu_meta = pmt::dict_add(m_pdu_meta, pmt::intern("size"), pmt::from_long(m_chunk_size));
    pmt_t::pmt_t 
    m_pdu_meta = pmt::dict_add(m_pdu_meta, pmt::intern("size"), pmt::from_long(m_chunk_size));
    */
    m_pdu_vector = pmt::make_c32vector(m_chunk_size, gr_complex(0.f,0.f));
}

/*
 * Our virtual destructor.
 */
trigger_impl::~trigger_impl() {
}

void trigger_impl::set_threshold(float t) {
    m_thr = t;
}

void trigger_impl::send_message(){
    pmt::pmt_t dict = pmt::make_dict();
    dict = pmt::dict_add(dict, pmt::intern("type"), pmt::intern("iq"));
    dict = pmt::dict_add(dict, pmt::intern("size"), pmt::from_long(m_chunk_size));
    
    for (int i = 0; i < m_chunk_size; ++i) {
        pmt::c32vector_set(m_pdu_vector, i,  m_data.at(i) );
    }
    
    pmt::pmt_t msg = pmt::cons(dict, m_pdu_vector);
    message_port_pub(m_port, msg);
}

float pwr(const gr_complex* data, int num){
    float m;
    float* vec = (float*) volk_malloc(num * sizeof(float), volk_get_alignment());
    volk_32fc_magnitude_32f(vec, data, num);
    volk_32f_accumulator_s32f(&m, vec, num);
    return std::sqrt( m / (float) num );
}

float average(const float* ptr, const int num){
    float acc = 0.f;

    for (int i = 0; i < num; ++i) {
        acc += ptr[i];
    }
    return acc / (float) num;
}

int trigger_impl::work(int noutput_items,
                         gr_vector_const_void_star& input_items,
                         gr_vector_void_star& output_items)
{
    auto in = static_cast<const gr_complex*>(input_items[0]);
    auto t1 = static_cast<const float*>(input_items[1]);
    auto t2 = static_cast<const float*>(input_items[2]);

    if (m_state == WAITING) { // Waiting for trigger...
        for (int32_t idx = 0; idx < noutput_items; ++idx) {
            const bool trigger1 = (*t1 > m_thr);
            const bool trigger2 = (*t2 > m_thr);

            if (trigger1 && trigger2) {
                m_state = TRIGGERED;
                m_trig_count++;

                std::cout << "\nTriggered at: " << idx << ", out of " << noutput_items << " \n";
                // Collect
                int items_to_collect = std::min(noutput_items - idx - 1, m_chunk_size);
                for (int i = 0; i < items_to_collect; ++i) {
                    m_data.at(i) = *in++;
                }
                m_items_collected += items_to_collect;
                int rem = m_chunk_size - m_items_collected;
                //std::cout << "   collected " << items_to_collect << " acc: " << m_items_collected << "  rem: " << rem << "\n";
                if (rem == 0) { 
                    std::cout << "Completed\n"; 
                    m_items_collected = 0; 
                    m_state = WAITING;
                }
                m_total_items += idx;
                return noutput_items;
            }
            in++;
            t1++;
            t2++;
        }
        // I got nothing, drop everything and keep listening....
        m_total_items += noutput_items;
        return noutput_items;
    }
    else if (m_state == TRIGGERED){
        int rem = m_chunk_size - m_items_collected;

        if (rem == 0) {
            std::cout << "Completed\n\n";
            send_message();
            m_items_collected = 0;
            m_state = WAITING;
            return 0;
        }
        // Still collecting...
        int items_to_collect = std::min(noutput_items, rem);
        //std::cout << "   COLLECTING: attempting to collect  " << items_to_collect << "\n";
        for (int i = 0; i < items_to_collect; ++i) {
            m_data.at(i + m_items_collected) = *in++;
        }
        m_items_collected += items_to_collect;
        //std::cout << "   collected " << items_to_collect << " acc: " << m_items_collected << "  rem: " << rem - items_to_collect<< "\n";
        return items_to_collect;
    }
    else {
        // What are we even doing here...
        return noutput_items;
    }
}

} /* namespace droneid */
} /* namespace gr */
