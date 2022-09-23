/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_TRIG_SAVE_H
#define INCLUDED_DRONEID_TRIG_SAVE_H

#include <gnuradio/droneid/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace droneid {

/*!
 * \brief <+description of block+>
 * \ingroup droneid
 *
 */
class DRONEID_API trig_save : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<trig_save> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::trig_save.
     *
     * To avoid accidental use of raw pointers, droneid::trig_save's
     * constructor is in a private implementation
     * class. droneid::trig_save::make is the public interface for
     * creating new instances.
     */
    static sptr make(int chunk_size, float threshold, const char* filename);
    virtual void set_threshold(float /*threshold*/) = 0;
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_TRIG_SAVE_H */
