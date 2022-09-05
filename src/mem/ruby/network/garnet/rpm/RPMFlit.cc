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


#include "mem/ruby/network/garnet/rpm/RPMFlit.hh"

#include "base/intmath.hh"
#include "debug/RubyNetwork.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMFlit::RPMFlit(int packet_id, int id,
        int vc, int vnet, RouteInfo route, int size,
        MsgPtr msg_ptr, int MsgSize, uint32_t bWidth,
        Tick curTime, int cur_router)
    : flit(packet_id, id, vc, vnet, route,
            size, msg_ptr, MsgSize, bWidth,
            curTime, cur_router)
{}

RPMFlit*
RPMFlit::replicate(GarnetNetwork *net_ptr, std::set<int> dest_routers)
{
    //FIXME: Here we need to update vnet in flit if it goes to north
    // and the current router is source. For now I couldn't found the
    // deadlock issue yet...
    RPMFlit* f = new RPMFlit();
    *f = *this; // it is okay because there is not pointer.
    f->m_msg_ptr = f->m_msg_ptr->clone();

    std::vector<NodeID> all_dest = m_route.net_dest.getAllDest();
    std::set<int> dest_nis;
    for (auto dest_ni_id : all_dest) {
        int dest_router_id = net_ptr->get_router_id(dest_ni_id, f->m_vnet);
        if (dest_routers.find(dest_router_id) != dest_routers.end()) {
            dest_nis.insert(dest_ni_id);
        } else {
            f->m_route.dest_nis.erase(dest_ni_id);
            f->m_route.dest_routers.erase(dest_router_id);
        }
    }

    f->m_route.net_dest.removeDestsExcept(dest_nis);
    return f;
}

void
RPMFlit::print(std::ostream& out) const
{
    out << "[flit:: ";
    out << "PacketId=" << m_packet_id << " ";
    out << "Id=" << m_id << " ";
    out << "Type=" << m_type << " ";
    out << "Size=" << m_size << " ";
    out << "Vnet=" << m_vnet << " ";
    out << "VC=" << m_vc << " ";
    out << "Src NI=" << m_route.src_ni << " ";
    out << "Src Router=" << m_route.src_router << " ";
    out << "Dest NI=" << m_route.dest_ni << " ";
    out << "Dest Router=" << m_route.dest_router << " ";
    std::string ss = "";
    for (auto dest_ni : m_route.dest_nis) {
        ss += std::to_string(dest_ni) + " ";
    }
    out << "Dest NIS=" << ss << " ";

    ss = "";
    for (auto dest_router : m_route.dest_routers) {
        ss += std::to_string(dest_router) + " ";
    }
    out << "Dest Routers=" << ss << " ";
    out << "Set Time=" << m_time << " ";
    out << "Width=" << m_width<< " ";
    out << "Cur Router=" << m_cur_router << " ";
    out << "]";
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
