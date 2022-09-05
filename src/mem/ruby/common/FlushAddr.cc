/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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

#include "mem/ruby/common/FlushAddr.hh"

#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/slicc_interface/RubySlicc_Util.hh"

//#include "debug/Clflush.hh"

namespace gem5
{

namespace ruby
{

void
FlushAddr::addAddrToTable(int flush_num, Addr addr, MachineType type, int low_bit, int num_bit, int id)
{
    for (int i = 0; i < flush_num; i++) {
        MachineID newElement = mapAddressToRange(addr, type, low_bit, num_bit, intToID(id));
        int nid = newElement.num;

        unordered_map<int, vector<Addr>>::const_iterator got = addr_table.find(nid);
        if (got != addr_table.end()) {
            addr_table[nid].push_back(addr);
        }
        else {
            vector<Addr> tmp;
            tmp.push_back(addr);
            addr_table.insert({nid, tmp});
        }
        addr = addr + 64;
    }
}

Addr
FlushAddr::getAddr(MachineID mid, int index) const
{
    int nid = mid.num;
    unordered_map<int, vector<Addr>>::const_iterator got = addr_table.find(nid);
    assert(got != addr_table.end());
    Addr ret_addr = addr_table.at(nid)[index];
    return ret_addr;
}

bool
FlushAddr::moreFlushToEnqueue(MachineID mid, int index) const
{
    int nid = mid.num;
    unordered_map<int, vector<Addr>>::const_iterator got = addr_table.find(nid);
    assert(got != addr_table.end());
    if ((addr_table.at(nid).size() - 1) > index)
        return true;
    else
        return false;
}

/*void
FlushAddr::printTable() const
{
    for (auto it : addr_table) {
        DPRINTF(Clflush, "NodeID: %d\n", it.first);
        for (int i = 0; i < it.second.size(); i++) {
            DPRINTF(Clflush, "%#x \n", it.second[i]);
        }
    }
}*/


void
FlushAddr::print(std::ostream& out) const
{
    out << "not implemented" << endl;
}

} // namespace ruby
} // namespace gem5
