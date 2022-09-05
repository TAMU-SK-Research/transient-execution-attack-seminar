/*
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


#include "mem/ruby/network/garnet/rpm/RPMInputUnit.hh"

#include "debug/RubyNetwork.hh"
#include "debug/RubyNetworkPacket.hh"
#include "mem/ruby/network/garnet/rpm/RPMSetAsideBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMInputUnit::RPMInputUnit(int id, PortDirection direction, Router *router)
    : InputUnit(id, direction, router)
{
    m_rpm_router = dynamic_cast<RPMRouter*>(router);
    assert(m_rpm_router != nullptr);
}

void
RPMInputUnit::wakeup()
{
    flit *t_flit;
    if (m_in_link->isReady(curTick())) {

        t_flit = m_in_link->consumeLink();
        DPRINTF(RubyNetwork, "Router[%d] Consuming:%s Width: %d Flit:%s\n",
        m_router->get_id(), m_in_link->name(),
        m_router->getBitWidth(), *t_flit);
        t_flit->set_cur_router(m_router->get_id());
        NDPRINTF(RubyNetworkPacket, t_flit,
                m_router->get_net_ptr()->get_trace_packet_id(),
                "Router[%d] IN[%s(%d)] Consuming a flit %s\n",
                t_flit->get_cur_router(), m_direction, m_id, *t_flit);
        assert(t_flit->m_width == m_router->getBitWidth());
        int vc = t_flit->get_vc();
        t_flit->increment_hops(); // for stats

        outport2dests_t outport2dests = m_rpm_router->route_compute(
                t_flit->get_route(), m_direction);

        RPMFlit* t_rpmflit = dynamic_cast<RPMFlit*>(t_flit);
        assert(t_rpmflit != nullptr);
        std::vector<ReplicaInfo> replicas;
        for (auto o2dests: outport2dests) {
            int outport = o2dests.first;
            RPMFlit* f = t_rpmflit->replicate(
                    m_router->get_net_ptr(), o2dests.second);
            assert(f != NULL);

            if (m_rpm_router->get_outport_dirn(outport) == "Local") {
                int vnet = vc/m_vc_per_vnet;
                int virtual_networks = m_router->get_net_ptr()
                    ->getNumberOfVirtualNetworks();
                if (vnet >= (virtual_networks / 2)) {
                    vnet = vnet - (virtual_networks / 2);
                }
                outport = m_rpm_router->lookupRoutingTable(
                        vnet, f->get_route().net_dest);
                assert(m_rpm_router->get_outport_dirn(outport) == "Local");
            }

            Cycles pipe_stages = m_router->get_pipe_stages();
            if (pipe_stages == 1) {
                f->advance_stage(SA_, curTick());
            } else {
                assert(pipe_stages > 1);
                Cycles wait_time = pipe_stages - Cycles(1);
                f->advance_stage(SA_, m_router->clockEdge(wait_time));
            }
            replicas.push_back(ReplicaInfo(m_id, outport, f));
        }
        delete t_flit;
        assert(replicas.size() > 0);

        int outport = replicas.front().outport;
        t_flit = replicas.front().rpmFlit;
        replicas.erase(replicas.begin());

        if ((t_flit->get_type() == HEAD_) ||
            (t_flit->get_type() == HEAD_TAIL_)) {

            assert(virtualChannels[vc].get_state() == IDLE_);
            set_vc_active(vc, curTick());

            // Update output port in VC
            // All flits in this packet will use this output port
            // The output port field in the flit is updated after it wins SA
            grant_outport(vc, outport);

        } else {
            assert(virtualChannels[vc].get_state() == ACTIVE_);
        }

        // Buffer the flit
        NDPRINTF(RubyNetworkPacket, t_flit,
                m_router->get_net_ptr()->get_trace_packet_id(),
                "Router[%d] IN[%s(%d)] Inserting a flit to VC %d %s\n",
                t_flit->get_cur_router(), m_direction, m_id, vc, *t_flit);
        virtualChannels[vc].insertFlit(t_flit);

        for (auto replica : replicas) {
            NDPRINTF(RubyNetworkPacket, t_flit,
                    m_router->get_net_ptr()->get_trace_packet_id(),
                    "Router[%d] IN[%s(%d)] "
                    "Inserting a flit to VC(rpm) %d %s\n",
                    replica.rpmFlit->get_cur_router(),
                    m_direction, m_id, vc, *(replica.rpmFlit));
        }
        insertReplicas(m_id, replicas);

        int vnet = vc/m_vc_per_vnet;
        // number of writes same as reads
        // any flit that is written will be read only once
        m_num_buffer_writes[vnet]++;
        m_num_buffer_reads[vnet]++;

        Cycles pipe_stages = m_router->get_pipe_stages();
        if (pipe_stages == 1) {
            // 1-cycle router
            // Flit goes for SA directly
            t_flit->advance_stage(SA_, curTick());
        } else {
            assert(pipe_stages > 1);
            // Router delay is modeled by making flit wait in buffer for
            // (pipe_stages cycles - 1) cycles before going for SA

            Cycles wait_time = pipe_stages - Cycles(1);
            t_flit->advance_stage(SA_, m_router->clockEdge(wait_time));

            // Wakeup the router in that cycle to perform SA
            m_router->schedule_wakeup(Cycles(wait_time));
        }

        if (m_in_link->isReady(curTick())) {
            m_router->schedule_wakeup(Cycles(1));
        }
    }
}

void
RPMInputUnit::insertReplicas(const int inport,
        std::vector<ReplicaInfo> replicas)
{
    int selected_setaside_id = m_rpm_router->selectSetAsideBuffer(inport);
    auto input_unit = m_rpm_router->getInputUnit(selected_setaside_id);

    RPMSetAsideBuffer* rpm_buffer
        = dynamic_cast<RPMSetAsideBuffer*>(input_unit);
    rpm_buffer->insertReplicas(replicas);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
