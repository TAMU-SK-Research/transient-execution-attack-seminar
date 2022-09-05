/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/rpm/RPMRouter.hh"

#include "mem/ruby/network/garnet/rpm/RPMSetAsideBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMRouter::RPMRouter(const Params &p)
    : Router(p), m_rpmRoutingUnit(nullptr)
{}

void
RPMRouter::init()
{
    //---------------------------------------------------------
    // RPM specifics.
    //---------------------------------------------------------
    m_rpmRoutingUnit = dynamic_cast<RPMRoutingUnit*>(routingUnit);
    assert(m_rpmRoutingUnit != nullptr);

    // Add a set-aside buffer to the router
    int port_id;
    m_orig_num_inports = port_id = m_input_unit.size();
    m_num_setaside_buffers = m_input_unit.size();
    for (int i = 0; i < m_num_setaside_buffers; i++) {
        port_id += i;
        m_input_unit.push_back(std::shared_ptr<InputUnit>(
                    new RPMSetAsideBuffer(port_id, "RPM", this)));
        routingUnit->addInDirection("RPM", port_id);
    }
    Router::init();
}

outport2dests_t
RPMRouter::route_compute(RouteInfo route, PortDirection direction)
{
    return m_rpmRoutingUnit->outportComputeRPM(route, direction);
}

int
RPMRouter::lookupRoutingTable(int vnet, NetDest net_dest)
{
    return m_rpmRoutingUnit->lookupRoutingTable(vnet, net_dest);
}

PortDirection
RPMRouter::get_outport_dirn(const int outport)
{
    return m_rpmRoutingUnit->get_outport_dirn(outport);
}

PortDirection
RPMRouter::get_inport_dirn(const int inport)
{
    return m_rpmRoutingUnit->get_inport_dirn(inport);
}

bool
RPMRouter::isNorthPartition(std::set<int> dests)
{
    return m_rpmRoutingUnit->isNorthPartition(dests);
}

int RPMRouter::selectSetAsideBuffer(const int inport)
{
    assert(inport < m_orig_num_inports);
    int selected_inport = inport + m_orig_num_inports;
    assert(selected_inport < m_input_unit.size());
    return selected_inport;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
