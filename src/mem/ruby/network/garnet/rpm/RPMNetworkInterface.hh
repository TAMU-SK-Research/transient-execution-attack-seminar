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

#ifndef __MEM_RUBY_NETWORK_GARNET_0_RPM_NETWORKINTERFACE_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_RPM_NETWORKINTERFACE_HH__

#include "mem/ruby/network/garnet/NetworkInterface.hh"
#include "mem/ruby/network/garnet/rpm/RPMGarnetNetwork.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "params/RPMGarnetNetworkInterface.hh"

namespace gem5
{

namespace ruby
{
namespace garnet
{

class RPMNetworkInterface : public NetworkInterface
{
    public:
        typedef GarnetNetworkInterfaceParams Params;
        RPMNetworkInterface(const Params &p);
        ~RPMNetworkInterface() = default;
        void init_net_ptr(GarnetNetwork *net_ptr);

        void wakeup();

    private:
        void checkStallQueue();
        int calculateVC(int vnet);
        bool flitisizeMessage(MsgPtr msg_ptr, int vnet);

        RPMGarnetNetwork* m_rpm_net_ptr;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5
#endif // __MEM_RUBY_NETWORK_GARNET_0_RPM_NETWORKINTERFACE_HH__
