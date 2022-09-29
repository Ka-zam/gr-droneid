/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_SINGLE_TRIGGER_IMPL_H
#define INCLUDED_DRONEID_SINGLE_TRIGGER_IMPL_H

#include <gnuradio/droneid/single_trigger.h>

namespace gr {
namespace droneid {

class single_trigger_impl : public single_trigger
{
private:
	
public:
    single_trigger_impl(float threshold, int chunk_size);
    ~single_trigger_impl();

    void set_threshold(float /*threshold*/) override;
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_SINGLE_TRIGGER_IMPL_H */
