/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_SAVE_MSG_H
#define INCLUDED_DRONEID_SAVE_MSG_H

#include <gnuradio/block.h>
#include <gnuradio/droneid/api.h>

namespace gr {
namespace droneid {

/*!
 * \brief Save received messages
 * \ingroup droneid
 *
 */
class DRONEID_API save_msg : virtual public gr::block
{
public:
    typedef std::shared_ptr<save_msg> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of droneid::save_msg.
     *
     * To avoid accidental use of raw pointers, droneid::save_msg's
     * constructor is in a private implementation
     * class. droneid::save_msg::make is the public interface for
     * creating new instances.
     */
    static sptr make(std::string filename);
    virtual void set_filename(std::string /*filename*/) = 0;    
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_SAVE_MSG_H */
