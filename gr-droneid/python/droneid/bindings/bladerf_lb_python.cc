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
/* BINDTOOL_HEADER_FILE(bladerf_lb.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(4ed8a7b1bb53fe6211754f13aae0b99a)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/droneid/bladerf_lb.h>
// pydoc.h is automatically generated in the build directory
#include <bladerf_lb_pydoc.h>

void bind_bladerf_lb(py::module& m)
{

    using bladerf_lb    = ::gr::droneid::bladerf_lb;


    py::class_<bladerf_lb, gr::block, gr::basic_block,
        std::shared_ptr<bladerf_lb>>(m, "bladerf_lb", D(bladerf_lb))

        .def(py::init(&bladerf_lb::make),
           py::arg("samp_rate"),
           D(bladerf_lb,make)
        )
        



        ;




}







