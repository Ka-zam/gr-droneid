/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "save_msg_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

using namespace std::chrono;

save_msg::sptr save_msg::make(std::string filename)
{
    return gnuradio::make_block_sptr<save_msg_impl>(filename);
}

/*
 * The private constructor
 */
save_msg_impl::save_msg_impl(std::string filename)
    : gr::block("save_msg", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0))
{
    message_port_register_in(pmt::mp("in"));
    set_msg_handler(pmt::mp("in"), [this](const pmt::pmt_t& msg) { this->save(msg); });
    m_filestem = filename;
}
/*
 * Our virtual destructor.
 */
save_msg_impl::~save_msg_impl() {}

void save_msg_impl::save(const pmt::pmt_t& msg){
    if (pmt::is_pdu(msg)) {
        const auto& meta = pmt::car(msg);
        const auto& vector = pmt::cdr(msg);
        if (pmt::is_c32vector(vector)) {
            pmt::pmt_t not_found;
            int size = pmt::to_long(pmt::dict_ref(meta, pmt::mp("size"), not_found));
            
            std::vector<gr_complex> iq_vec;
            for (int i = 0; i < size; ++i) {
                iq_vec.push_back(pmt::c32vector_ref(vector, i ));
            }
            uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
			std::string fn = m_filestem + std::to_string(ms) + ".fc32";
			FILE* fp = fopen(fn.c_str(), "wb");
			fwrite((uint8_t *) iq_vec.data(), sizeof(gr_complex), size, fp );
			fclose(fp);
        }
    }	
}

void save_msg_impl::set_filename(std::string filename) { m_filestem = filename; }

} /* namespace droneid */
} /* namespace gr */
