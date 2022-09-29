/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "print_msg_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

print_msg::sptr print_msg::make() { return gnuradio::make_block_sptr<print_msg_impl>(); }

/*
 * The private constructor
 */
print_msg_impl::print_msg_impl()
    : gr::block("print_msg",
    gr::io_signature::make(0, 0, 0),
    gr::io_signature::make(0, 0, 0)) 
{
    message_port_register_in(pmt::mp("in"));
    set_msg_handler(pmt::mp("in"), [this](const pmt::pmt_t& msg) { this->print(msg); });    
}

/*
 * Our virtual destructor.
 */
print_msg_impl::~print_msg_impl() {}

void print_msg_impl::print(const pmt::pmt_t& msg) 
{
    if (pmt::is_pdu(msg)) {
        const auto& meta = pmt::car(msg);
        std::cout << pmt::write_string(meta) << "\n";
    }
}

} /* namespace droneid */
} /* namespace gr */
