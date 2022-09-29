/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "single_trigger_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace droneid {

single_trigger::sptr single_trigger::make(float threshold, int chunk_size)
{
    return gnuradio::make_block_sptr<single_trigger_impl>(threshold, chunk_size);
}


/*
 * The private constructor
 */
single_trigger_impl::single_trigger_impl(float threshold, int chunk_size)
    : gr::sync_block("single_trigger",
                     gr::io_signature::make2(2, 2 , sizeof(gr_complex), sizeof(float)),
                     gr::io_signature::make(0, 0, 0))
{

}

/*
 * Our virtual destructor.
 */
single_trigger_impl::~single_trigger_impl() {}

void single_trigger_impl::set_threshold(float /*threshold*/) {
    
}


int single_trigger_impl::work(int noutput_items,
                              gr_vector_const_void_star& input_items,
                              gr_vector_void_star& output_items)
{
    auto in = static_cast<const gr_complex*>(input_items[0]);
    auto t1 = static_cast<const float*>(input_items[1]);

    in += noutput_items;
    t1 += noutput_items;

    return noutput_items;
}

} /* namespace droneid */
} /* namespace gr */
