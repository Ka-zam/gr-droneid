/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_DUAL_TRIGGER_H
#define INCLUDED_DRONEID_DUAL_TRIGGER_H

#include <gnuradio/droneid/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace droneid {

/*!
 * \brief Symbol 4 and 6 dual trigger
 * \ingroup droneid
 *
 */
class DRONEID_API dual_trigger : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<dual_trigger> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::dual_trigger.
     *
     * To avoid accidental use of raw pointers, droneid::dual_trigger's
     * constructor is in a private implementation
     * class. droneid::dual_trigger::make is the public interface for
     * creating new instances.
     */
    static sptr make(float fc, float threshold, int chunk_size);
    virtual void set_threshold(float /*threshold*/) = 0;
    virtual void set_fc(float /*fc*/) = 0;    
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_DUAL_TRIGGER_H */
