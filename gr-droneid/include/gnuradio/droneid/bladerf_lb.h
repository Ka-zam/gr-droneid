/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_BLADERF_LB_H
#define INCLUDED_DRONEID_BLADERF_LB_H

#include <gnuradio/sync_block.h>
#include <gnuradio/droneid/api.h>

namespace gr {
namespace droneid {

/*!
 * \brief Test bladerf throughput
 * \ingroup droneid
 *
 */
class DRONEID_API bladerf_lb : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<bladerf_lb> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::bladerf_lb.
     *
     * To avoid accidental use of raw pointers, droneid::bladerf_lb's
     * constructor is in a private implementation
     * class. droneid::bladerf_lb::make is the public interface for
     * creating new instances.
     */
    static sptr make(int samp_rate);
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_BLADERF_LB_H */
