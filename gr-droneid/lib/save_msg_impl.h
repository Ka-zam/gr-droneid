/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_SAVE_MSG_IMPL_H
#define INCLUDED_DRONEID_SAVE_MSG_IMPL_H

#include <gnuradio/droneid/save_msg.h>

namespace gr {
namespace droneid {

class save_msg_impl : public save_msg
{
private:
	uint64_t m_count;
	std::string m_filestem;
    void save(const pmt::pmt_t& msg);
public:
    save_msg_impl(std::string filename);
    ~save_msg_impl();
    void set_filename(std::string filename) override;
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_SAVE_MSG_IMPL_H */
