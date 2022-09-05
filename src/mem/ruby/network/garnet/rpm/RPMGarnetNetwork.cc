/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/rpm//RPMGarnetNetwork.hh"
#include "sim/system.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RPMGarnetNetwork::RPMGarnetNetwork(const Params &p)
    : GarnetNetwork(p),
    deadlockCheckEvent([this]{ wakeup(); }, "RPMGarnetNetwork multicast check")
{
}

void
RPMGarnetNetwork::init()
{
    GarnetNetwork::init();
    m_multicast_threshold = Cycles(5000000);
}

void
RPMGarnetNetwork::add_multicast_packet(
        const int packet_id, Cycles inj_time, std::set<int> dests)
{
    std::string ss = "";
    for (auto dest : dests) ss += std::to_string(dest) + " ";
    assert(m_multicast_table.find(packet_id) == m_multicast_table.end());

    // See if we should schedule a deadlock check
    if (!deadlockCheckEvent.scheduled()) {
        schedule(deadlockCheckEvent, clockEdge(m_multicast_threshold));
    }

    m_multicast_table[packet_id] = std::make_pair(inj_time, dests);
}

void
RPMGarnetNetwork::remove_multicast_dest(const int packet_id,
            const int dest)
{
    assert(m_multicast_table.find(packet_id) != m_multicast_table.end());
    std::pair<Cycles, std::set<int>> cNd = m_multicast_table[packet_id];
    assert(cNd.second.find(dest) != cNd.second.end());
    cNd.second.erase(dest);
    if (cNd.second.size() > 0) {
        m_multicast_table[packet_id] = cNd;
    } else {
        m_multicast_table.erase(packet_id);
    }
}

void
RPMGarnetNetwork::wakeup()
{
    Cycles current_time = curCycle();
    for (auto p2cNd : m_multicast_table) {
        int packet_id = p2cNd.first;
        Cycles inj_time = p2cNd.second.first;
        if (current_time - inj_time < m_multicast_threshold) {
            continue;
        }

        std::string ss = "";
        for (auto dest : p2cNd.second.second) {
            ss += std::to_string(dest) + " ";
        }

        panic("A multicast packet %d has an issue."
              " injection time: %d, current time: %d"
              " The following destinations haven't"
              " received the packet for %d (threshold %d)\n - %s\n",
                packet_id,
                inj_time * clockPeriod(), current_time * clockPeriod(),
                (current_time * clockPeriod()) - (inj_time * clockPeriod()) ,
                m_multicast_threshold, ss);
    }

    // If there are still outstanding requests, keep checking
    if (m_multicast_table.size() > 0) {
        schedule(deadlockCheckEvent, clockEdge(m_multicast_threshold));
    }
}

std::string
RPMGarnetNetwork::get_multicast_status()
{
    std::string ss = "\nmulticast_status\n";
    for (auto p2cNd : m_multicast_table) {
        int packet_id = p2cNd.first;
        Cycles inj_time = p2cNd.second.first;
        ss += "packet_id: "
            + std::to_string(packet_id)
            + ", inj_time: "
            + std::to_string(inj_time * clockPeriod())
            + ", dests: ";
        for (auto dest : p2cNd.second.second) {
            ss += std::to_string(dest) + " ";
        }
        ss += "\n";
    }

    return ss;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
