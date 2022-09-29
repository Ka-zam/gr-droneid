/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "msg_trigger_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

msg_trigger::sptr msg_trigger::make(float threshold)
{
    return gnuradio::make_block_sptr<msg_trigger_impl>(threshold);
}
/*
 * The private constructor
 */
msg_trigger_impl::msg_trigger_impl(float threshold)
    : gr::block("msg_trigger",
    gr::io_signature::make(0, 0, 0),gr::io_signature::make(0, 0, 0)),
    m_port(pmt::mp("pdu"))
{
    m_thr = threshold;
    message_port_register_out(m_port);
    message_port_register_in(pmt::mp("in"));
    set_msg_handler(pmt::mp("in"), [this](const pmt::pmt_t& msg) { this->handle_msg(msg); });
}
/*
 * Our virtual destructor.
 */
msg_trigger_impl::~msg_trigger_impl() {}

void msg_trigger_impl::set_threshold(float t) {
    m_thr = t;
}

void msg_trigger_impl::handle_msg(const pmt::pmt_t& msg) {
    pmt::pmt_t new_msg = msg;
    if (pmt::is_pdu(msg)) {
        const auto& meta = pmt::car(msg);
        const auto& vector = pmt::cdr(msg);
        std::cout << pmt::write_string(meta) << "\n";
        /*
         * Here we should test for symbol 6
         */
        if (pmt::is_c32vector(vector)) {
            size_t num;
            const gr_complex* c_ptr = pmt::c32vector_elements(vector, num);

            /*
            pmt_t pmt::(dtype)vector_ref(pmt_t vector, size_t k)

            for (int i = 0; i < m_chunk_size; ++i) {
                pmt::c32vector_set(m_pdu_vector, i,  m_data.at(i) );
            } 
            */       
        }
    }
    message_port_pub(m_port, new_msg);
}

} /* namespace droneid */
} /* namespace gr */
