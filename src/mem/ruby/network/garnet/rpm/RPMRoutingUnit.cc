/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
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


#include "mem/ruby/network/garnet/rpm/RPMRoutingUnit.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/Router.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMRoutingUnit::RPMRoutingUnit(Router *router)
    : RoutingUnit(router) {}

int
RPMRoutingUnit::outportComputeCustom(RouteInfo route,
        int inport,
        PortDirection inport_dirn)
{
    return outportComputeXY(route, inport, inport_dirn);
}

outport2dests_t
RPMRoutingUnit::outportComputeRPM(RouteInfo route,
                PortDirection inport_dirn)
{
    // Step 1
    // Collect all partitions of the destinations.
    std::vector<NodeID> all_dest = route.net_dest.getAllDest();
    std::vector<bool> dest_parts(NUM_PARTITION, false);
    for (auto dest_ni_id : all_dest) {
        int dest_router_id
            = m_router->get_net_ptr()->get_router_id(dest_ni_id, route.vnet);
        Partition p = getPartitionOf(dest_router_id);
        dest_parts[p] = true;
    }

    // Step 2
    // Calculate outports.
    std::set<int> outports;
    if (dest_parts[E_] ||
            (dest_parts[SE_] && !dest_parts[S_] && !dest_parts[SW_])) {
        outports.insert(m_outports_dirn2idx["East"]);
    }

    if (dest_parts[N_]
    || (dest_parts[NE_]
            && (!dest_parts[E_] || (!dest_parts[SW_] && dest_parts[SE_])))
    || (dest_parts[NE_] && dest_parts[NW_])) {
        outports.insert(m_outports_dirn2idx["North"]);
    }

    if (dest_parts[W_]
    || (dest_parts[NW_] && !dest_parts[N_] && !dest_parts[NE_])) {
        outports.insert(m_outports_dirn2idx["West"]);
    }

    if (dest_parts[S_]
    || (dest_parts[SW_]
        && (!dest_parts[W_] || (!dest_parts[NE_] && dest_parts[SW_])))
    || (dest_parts[SW_] && dest_parts[SE_])) {
        outports.insert(m_outports_dirn2idx["South"]);
    }

    if (dest_parts[L_]) {
        // Note: There are many outport of "Local".
        // Lookup RoutingTable to find the corressponding outport.
        outports.insert(m_outports_dirn2idx["Local"]);
    }

    int outport_east  = m_outports_dirn2idx["East"];
    int outport_west  = m_outports_dirn2idx["West"];
    int outport_south = m_outports_dirn2idx["South"];
    int outport_north = m_outports_dirn2idx["North"];
    int outport_local = m_outports_dirn2idx["Local"];

    outport2dests_t outport2dests;

    for (auto dest_ni_id : all_dest) {
        int dest_router_id
            = m_router->get_net_ptr()->get_router_id(dest_ni_id, route.vnet);
        Partition p = getPartitionOf(dest_router_id);
        switch (p) {
            case NE_:
                if (outports.find(outport_north) != outports.end()) {
                    outport2dests[outport_north].insert(dest_router_id);
                } else if (outports.find(outport_east) != outports.end()) {
                    outport2dests[outport_east].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case N_:
                if (outports.find(outport_north) != outports.end()) {
                    outport2dests[outport_north].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case NW_:
                if (outports.find(outport_north) != outports.end()) {
                    outport2dests[outport_north].insert(dest_router_id);
                } else if (outports.find(outport_west) != outports.end()) {
                    outport2dests[outport_west].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case W_:
                if (outports.find(outport_west) != outports.end()) {
                    outport2dests[outport_west].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case SW_:
                if (outports.find(outport_south) != outports.end()) {
                    outport2dests[outport_south].insert(dest_router_id);
                } else if (outports.find(outport_west) != outports.end()) {
                    outport2dests[outport_west].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case S_:
                if (outports.find(outport_south) != outports.end()) {
                    outport2dests[outport_south].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case SE_:
                if (outports.find(outport_south) != outports.end()) {
                    outport2dests[outport_south].insert(dest_router_id);
                } else if (outports.find(outport_east) != outports.end()) {
                    outport2dests[outport_east].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case E_:
                if (outports.find(outport_east) != outports.end()) {
                    outport2dests[outport_east].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            case L_:
                if (outports.find(outport_local) != outports.end()) {
                    // NOTE:
                    // There are many outports of "Local"
                    // Find a correct outport using RoutingTable
                    outport2dests[outport_local].insert(dest_router_id);
                } else {
                    panic("error");
                }
                break;
            default:
                panic("check this line");
        }

    }

    return outport2dests;
}

PortDirection
RPMRoutingUnit::get_outport_dirn(const int outport)
{
    return m_outports_idx2dirn[outport];
}

PortDirection
RPMRoutingUnit::get_inport_dirn(const int inport)
{
    return m_inports_idx2dirn[inport];
}

bool
RPMRoutingUnit::isNorthPartition(std::set<int> dests)
{
    //int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();

    int router_id = m_router->get_id();
    //int router_x = router_id % num_cols;
    int router_y = router_id / num_cols;

    for (auto dest_id: dests) {
        //int dest_x = dest_id % num_cols;
        int dest_y = dest_id / num_cols;
        if (dest_y > router_y) {
            return true;
        }
    }
    return false;
}

Partition
RPMRoutingUnit::getPartitionOf(int dest_id)
{
    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int router_id = m_router->get_id();
    int router_x = router_id % num_cols;
    int router_y = router_id / num_cols;

    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    Partition p = UNKNOWN_PARTITION;

    if (router_y < dest_y)  {
        if (router_x < dest_x) {
            p = NE_;
        } else if (router_x == dest_x) {
            p = N_;
        } else if (router_x > dest_x) {
            p = NW_;
        }
    } else if (router_y == dest_y) {
        if (router_x < dest_x) {
            p = E_;
        } else if (router_x > dest_x) {
            p = W_;
        } else {
            p = L_;
        }
    } else if (router_y > dest_y) {
        if (router_x < dest_x) {
            p = SE_;
        } else if (router_x == dest_x) {
            p = S_;
        } else if (router_x > dest_x) {
            p = SW_;
        }
    }

    if (p == UNKNOWN_PARTITION) {
        panic("Unknown partition from (%d,%d) to (%d,%d)\n",
                router_x, router_y, dest_x, dest_y);
    }

    return p;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
