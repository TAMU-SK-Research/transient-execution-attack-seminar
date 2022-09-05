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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_RPM_COMMONTYPES_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_RPM_COMMONTYPES_HH__

#include <map>
#include <set>

#include "mem/ruby/network/garnet/rpm/RPMFlit.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

enum Partition
{
    NE_                 = 0,
    N_                  = 1,
    NW_                 = 2,
    W_                  = 3,
    SW_                 = 4,
    S_                  = 5,
    SE_                 = 6,
    E_                  = 7,
    L_                  = 8,
    NUM_PARTITION       = 9,
    UNKNOWN_PARTITION   = 10
};

typedef std::map<int, std::set<int>> outport2dests_t;

struct ReplicaInfo
{
    ReplicaInfo(int _inport, int _outport, RPMFlit* _rpmFlit)
        : inport(_inport), outport(_outport), rpmFlit(_rpmFlit) {}
    int inport;
    int outport;
    RPMFlit* rpmFlit;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_GARNET_0_COMMONTYPES_HH__
