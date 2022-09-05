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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_RPM_GARNETNETWORK_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_RPM_GARNETNETWORK_HH__

#include "mem/ruby/network/garnet/GarnetNetwork.hh"
#include "params/RPMGarnetNetwork.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

class RPMGarnetNetwork : public GarnetNetwork
{
    public:
        typedef GarnetNetworkParams Params;
        RPMGarnetNetwork(const Params &p);
        ~RPMGarnetNetwork() = default;

        void init();

        // For deack lock detection
        void add_multicast_packet(
                const int packet_id, Cycles inj_time, std::set<int> dests);
        void remove_multicast_dest(
                const int packet_id, const int dest);
        virtual void wakeup();
        std::string get_multicast_status();
        EventFunctionWrapper deadlockCheckEvent;
    private:
        Cycles m_multicast_threshold;
        std::map<int, std::pair<Cycles, std::set<int>>> m_multicast_table;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_GARNET_0_RPM_GARNETNETWORK_HH__
