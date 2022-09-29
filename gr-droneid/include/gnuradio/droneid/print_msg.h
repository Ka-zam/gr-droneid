/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_PRINT_MSG_H
#define INCLUDED_DRONEID_PRINT_MSG_H

#include <gnuradio/droneid/api.h>
#include <gnuradio/block.h>

namespace gr {
namespace droneid {

/*!
 * \brief Print DJI droneid message statistics
 * \ingroup droneid
 *
 */
class DRONEID_API print_msg : virtual public block
{
public:
    typedef std::shared_ptr<print_msg> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::print_msg.
     *
     * To avoid accidental use of raw pointers, droneid::print_msg's
     * constructor is in a private implementation
     * class. droneid::print_msg::make is the public interface for
     * creating new instances.
     */
    static sptr make();
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_PRINT_MSG_H */
