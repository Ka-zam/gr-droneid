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
    message_port_register_out(m_port);
    //m_data.resize(m_chunk_size);
    m_chunk_size = chunk_size;
    m_thr = threshold;
    m_total_items = 0;
    m_state = WAITING;
    m_items_written = 0;
    m_count = 0;
    m_pdu_vector = pmt::PMT_NIL;
    m_pdu_meta = pmt::PMT_NIL;
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
    //m_pdu_meta = pmt::intern("iq");
    //m_pdu_vector = pmt::init_c32vector(m_data.size(), m_data);

    //pmt::pmt_t msg = pmt::cons(m_pdu_meta, m_pdu_vector);

    //message_port_pub(m_port, msg);
    //m_pdu_vector = pmt::PMT_NIL;
    //m_pdu_meta = pmt::PMT_NIL;
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
 
    if (m_state == COLLECTING){
        int rem = m_chunk_size - m_items_written;

        if (rem <= 0) {
            std::cout << "Write completed\n\n";
            send_message();
            m_items_written = 0;
            m_state = WAITING;
            return noutput_items;
        }

        int items_to_write = std::min(rem, noutput_items);
        m_items_written += items_to_write;
        std::cout << "   not done, wrote " << items_to_write << " acc: " << m_items_written << "  rem: " << rem - items_to_write<< "\n";
        // Write
        for (int i = 0; i < items_to_write; ++i) {
            //m_data.at(i + m_items_written) = *in++;
        }
        return noutput_items;
    }
    else { // Waiting for trigger...
        for (int32_t idx = 0; idx < noutput_items; ++idx) {
            in++;
            bool trigger1 = (*t1 > m_thr);
            bool trigger2 = (*t2 > m_thr);
            }
            if (trigger1 && trigger2) {
                m_count++;
                m_state = COLLECTING;
                m_total_items += idx;
                /*
                float nse1, nse2;
                if(idx < noutput_items - 50) { 
                    nse1 = average(t1 + 25, 25); 
                    nse2 = average(t2 + 25, 25); 
                } else {
                    nse1 = average(t1 - 50, 25); 
                    nse2 = average(t2 - 50, 25);                     
                }
                */
                std::cout << "\nTriggered at: " << idx << ", out of " << noutput_items << " \n";
                std::cout << "     t1:" << *t1 << " t2: " << *t2 << "\n";
                //std::cout << "   t1:" << *t1 << " t2: " << *t2 << " noise: " << nse1 << "    " << nse2 << "\n";
                // Write
                int items_to_write = std::min(noutput_items - idx - 1, m_chunk_size);
                for (int i = 0; i < items_to_write; ++i) {
                    //m_data.at(i) = *in++;
                }
                m_items_written += items_to_write;
                int rem = m_chunk_size - m_items_written;
                std::cout << "   wrote " << items_to_write << " acc: " << m_items_written << "  rem: " << rem << "\n";
                if (rem == 0) {m_items_written = 0; m_state = WAITING;}
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
    return noutput_items;
}


    /*
    m_count++;
    if (m_count % 1000 == 0) {
        std::string fn = m_filename + "_" + std::to_string(m_count) + ".fc32";
        open(fn);
        m_count = 0;
    }
    return noutput_items;
    if (m_state == WRITING) {
        int items_to_write = std::max(m_chunk_size - m_items_written, noutput_items);
        // Write
        //m_items_written += items_to_write;
        m_items_written += fwrite(samples, sizeof(samples), items_to_write, m_fp);

        int rem = m_chunk_size - m_items_written;
        std::cout << "wrote " << items_to_write << " acc: " << m_items_written << "  rem: " << rem << "\n";
        if (m_items_written >= m_chunk_size) {
            std::cout << "Write completed\n\n";
            m_items_written = 0;
            m_state = WAITING;
            fflush(m_fp);
            fclose(m_fp);
        }
        return noutput_items;
        */

} /* namespace droneid */
} /* namespace gr */


/*
                samples += idx;
                float max = -100.f;
                for (int i = 0; i < items_to_write; ++i) {
                    float r = std::abs(std::real(*samples));
                    float m = std::abs(std::imag(*samples));
                    if (r > max) {max = r;};
                    if (m > max) {max = m;};
                    samples++;
                }
                std::cout << "max: " << max << " \n";
*/
