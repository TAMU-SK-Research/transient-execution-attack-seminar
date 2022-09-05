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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_RPM_SETASIDEBUFFER_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_RPM_SETASIDEBUFFER_HH__

#include <deque>

#include "debug/RubyNetwork.hh"
#include "debug/RubyNetworkPacket.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/rpm/RPMCommonTypes.hh"
#include "mem/ruby/network/garnet/rpm/RPMRouter.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

class RPMSetAsideBuffer : public InputUnit
{
    public:
        RPMSetAsideBuffer(
                int id, PortDirection direction, Router *router);
        ~RPMSetAsideBuffer() = default;

        void wakeup() {}

        void insertReplicas(std::vector<ReplicaInfo> replicas);

        bool need_stage(int invc, flit_stage stage, Tick time);
        int get_outport(int invc);
        int get_outvc(int invc);
        void grant_outvc(int invc, int outvc);
        flit* peekTopFlit();
        flit* getTopFlit(int vc);
        bool isReady(int invc, Tick curTime);

        // This input is nothing to do with the following functions.
        // Not to change the login in SwitchAllocator,
        // make them doing nothing.
        void set_state(VC_state_type m_state, Tick curTime) {}
        void set_vc_idle(int vc, Tick curTime) {}
        void set_vc_active(int vc, Tick curTime) {}
        void grant_outport(int vc, int outport) {}
        void increment_credit(int in_vc, bool free_signal, Tick curTime) {}

        // The following functions are associated with
        // Ordered protocol (isVNetOrdered).
        // RPM does not support it for now.
        Tick get_enqueue_time()          { panic(".."); }
        void set_enqueue_time(Tick time) { panic(".."); }
        VC_state_type get_state()        { panic(".."); }

    private:
        std::deque<ReplicaInfo> m_rpm_buffer;
        std::map<std::pair<int,int>, int> m_rpm_p2outvc;
        RPMRouter* m_rpm_router;


};
} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_GARNET_0_RPM_SETASIDEBUFFER_HH__
