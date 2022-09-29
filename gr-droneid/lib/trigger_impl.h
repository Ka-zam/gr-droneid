/* -*- c++ -*- */
/*
 * Copyright 2022 Magnus Lundmark.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DRONEID_TRIGGER_IMPL_H
#define INCLUDED_DRONEID_TRIGGER_IMPL_H

#include <gnuradio/droneid/trigger.h>
#include <volk/volk.h>

namespace gr {
namespace droneid {

class trigger_impl : public trigger
{
private:
	typedef enum { WAITING, TRIGGERED } state_t;
	float m_thr;
    float m_t1_last_sample;
	int64_t m_total_items;
	int32_t m_items_collected;
	int32_t m_trig_count;
	int32_t m_chunk_size;
    std::vector<gr_complex> m_data;
	state_t m_state;
    const pmt::pmt_t m_port;
    pmt::pmt_t m_pdu_vector;    
public:
    trigger_impl(float threshold, int chunk_size);
    ~trigger_impl();
    void set_threshold(float threshold) override;
    void send_message();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace droneid
} // namespace gr

#endif /* INCLUDED_DRONEID_TRIGGER_IMPL_H */
