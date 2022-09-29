/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_MSG_TRIGGER_IMPL_H
#define INCLUDED_DRONEID_MSG_TRIGGER_IMPL_H

#include <gnuradio/droneid/msg_trigger.h>

namespace gr {
namespace droneid {

class msg_trigger_impl : public msg_trigger
{
private:
	float m_thr;
    const pmt::pmt_t m_port;
    void handle_msg(const pmt::pmt_t& msg);
public:
    msg_trigger_impl(float threshold);
    ~msg_trigger_impl();
    void set_threshold(float t) override;
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_MSG_TRIGGER_IMPL_H */
