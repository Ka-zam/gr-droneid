/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_PRINT_MSG_IMPL_H
#define INCLUDED_DRONEID_PRINT_MSG_IMPL_H

#include <gnuradio/droneid/print_msg.h>

namespace gr {
namespace droneid {

class print_msg_impl : public print_msg
{
private:
    void print(const pmt::pmt_t& msg);
public:
    print_msg_impl();
    ~print_msg_impl();
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_PRINT_MSG_IMPL_H */
