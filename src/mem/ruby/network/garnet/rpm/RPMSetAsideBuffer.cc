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


#include "mem/ruby/network/garnet/rpm/RPMSetAsideBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMSetAsideBuffer::RPMSetAsideBuffer(
        int id, PortDirection direction, Router *router)
    : InputUnit(id, direction, router)
{
    m_rpm_router = dynamic_cast<RPMRouter*>(router);
    assert(m_rpm_router != nullptr);
}

void
RPMSetAsideBuffer::insertReplicas(
        std::vector<ReplicaInfo> replicas)
{
    for (auto replica : replicas) {
        m_rpm_buffer.push_back(replica);
    }
}

bool
RPMSetAsideBuffer::need_stage(
        int invc, flit_stage stage, Tick time)
{
    if (m_rpm_buffer.size() > 0) {
        flit* t_flit = m_rpm_buffer.front().rpmFlit;
        return t_flit->is_stage(stage, time);
    }
    return false;
}

int
RPMSetAsideBuffer::get_outport(int invc)
{
    assert(m_rpm_buffer.size() > 0);
    return m_rpm_buffer.front().outport;
}

int
RPMSetAsideBuffer::get_outvc(int invc)
{
    assert(m_rpm_buffer.size() > 0);

    flit* t_flit = m_rpm_buffer.front().rpmFlit;
    int outport  = m_rpm_buffer.front().outport;
    int pid      = t_flit->get_packet_id();
    auto key     = std::make_pair(pid, outport);

    if ((t_flit->get_type() == HEAD_) ||
            t_flit->get_type() == HEAD_TAIL_) {

        assert(m_rpm_p2outvc.find(key)
                == m_rpm_p2outvc.end());
        return -1;

    } else {
        assert(m_rpm_p2outvc.find(key)
                != m_rpm_p2outvc.end());
        return m_rpm_p2outvc[key];
    }
}

void
RPMSetAsideBuffer::grant_outvc(int invc, int outvc)
{
    assert(m_rpm_buffer.size() > 0);

    flit* t_flit = m_rpm_buffer.front().rpmFlit;
    int outport = m_rpm_buffer.front().outport;
    int pid = t_flit->get_packet_id();
    auto key = std::make_pair(pid, outport);

    if ((t_flit->get_type() == HEAD_) ||
            t_flit->get_type() == HEAD_TAIL_) {

        assert(m_rpm_p2outvc.find(key)
                == m_rpm_p2outvc.end());
        m_rpm_p2outvc[key] = outvc;
    } else {
        assert("Non-head flit can't set outvc");
    }
}

flit*
RPMSetAsideBuffer::peekTopFlit()
{
    assert(m_rpm_buffer.size() > 0);
    return m_rpm_buffer.front().rpmFlit;
}

flit*
RPMSetAsideBuffer::getTopFlit(int vc)
{
    assert(m_rpm_buffer.size() > 0);

    flit* t_flit = m_rpm_buffer.front().rpmFlit;
    int outport = m_rpm_buffer.front().outport;
    int pid = t_flit->get_packet_id();
    auto key = std::make_pair(pid, outport);

    if ((t_flit->get_type() == TAIL_) ||
            (t_flit->get_type() == HEAD_TAIL_)) {

        assert(m_rpm_p2outvc.find(key) != m_rpm_p2outvc.end());
        m_rpm_p2outvc.erase(key);
    }
    m_rpm_buffer.pop_front();
    return t_flit;
}

bool
RPMSetAsideBuffer::isReady(int invc, Tick curTime)
{
    // NOTE:
    // This function is used to check if whole packet is 
    // popped out from the input unit by SwitchAllocator::arbitrate_outports()
    // Since, RPMSetAsideBuffer can have multiple packets, this functions returns
    // false always.
    return false;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
