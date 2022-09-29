/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_MSG_TRIGGER_H
#define INCLUDED_DRONEID_MSG_TRIGGER_H

#include <gnuradio/droneid/api.h>
#include <gnuradio/block.h>

namespace gr {
namespace droneid {

/*!
 * \brief Check symbol 6 trigger condition
 * \ingroup droneid
 *
 */
class DRONEID_API msg_trigger : virtual public gr::block
{
public:
    typedef std::shared_ptr<msg_trigger> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::msg_trigger.
     *
     * To avoid accidental use of raw pointers, droneid::msg_trigger's
     * constructor is in a private implementation
     * class. droneid::msg_trigger::make is the public interface for
     * creating new instances.
     */
    static sptr make(float threshold);
    virtual void set_threshold(float /*threshold*/) = 0;    
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_MSG_TRIGGER_H */
