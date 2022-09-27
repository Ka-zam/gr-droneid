/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_TRIGGER_H
#define INCLUDED_DRONEID_TRIGGER_H

#include <gnuradio/droneid/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace droneid {

/*!
 * \brief Saves to file when both triggers above threshold
 * \ingroup droneid
 *
 */
class DRONEID_API trigger : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<trigger> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::trigger.
     *
     * To avoid accidental use of raw pointers, droneid::trigger's
     * constructor is in a private implementation
     * class. droneid::trigger::make is the public interface for
     * creating new instances.
     */
    static sptr make(float threshold, int chunk_size);
    virtual void set_threshold(float /*threshold*/) = 0;
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_TRIGGER_H */
