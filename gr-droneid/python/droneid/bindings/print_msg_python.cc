/*
 * Copyright 2022 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually edited  */
/* The following lines can be configured to regenerate this file during cmake      */
/* If manual edits are made, the following tags should be modified accordingly.    */
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(print_msg.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(907887a0c577c1ba8571f858a606dce8)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/droneid/print_msg.h>
// pydoc.h is automatically generated in the build directory
#include <print_msg_pydoc.h>

void bind_print_msg(py::module& m)
{

    using print_msg    = ::gr::droneid::print_msg;


    py::class_<print_msg, gr::block, gr::basic_block,
        std::shared_ptr<print_msg>>(m, "print_msg", D(print_msg))

        .def(py::init(&print_msg::make),
           D(print_msg,make)
        )
        



        ;




}







