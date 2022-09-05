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

#include "mem/ruby/network/garnet/rpm/RPMNetworkInterface.hh"

#include <cassert>
#include <cmath>

#include "base/cast.hh"
#include "debug/RubyNetwork.hh"
#include "debug/RubyNetworkPacket.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet/Credit.hh"
#include "mem/ruby/network/garnet/flitBuffer.hh"
#include "mem/ruby/network/garnet/rpm/RPMFlit.hh"
#include "mem/ruby/network/garnet/rpm/RPMRouter.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{
namespace ruby
{
namespace garnet
{

RPMNetworkInterface::RPMNetworkInterface(const Params &p)
    : NetworkInterface(p), m_rpm_net_ptr(nullptr) {}

void
RPMNetworkInterface::init_net_ptr(GarnetNetwork *net_ptr)
{
    m_rpm_net_ptr = dynamic_cast<RPMGarnetNetwork*>(net_ptr);
    assert(m_rpm_net_ptr != nullptr);
    NetworkInterface::init_net_ptr(net_ptr);
}

void
RPMNetworkInterface::wakeup()
{
    std::ostringstream oss;
    for (auto &oPort: outPorts) {
        oss << oPort->routerID() << "[" << oPort->printVnets() << "] ";
    }
    DPRINTF(RubyNetwork, "Network Interface %d connected to router:%s "
            "woke up. Period: %ld\n", m_id, oss.str(), clockPeriod());

    assert(curTick() == clockEdge());
    MsgPtr msg_ptr;
    Tick curTime = clockEdge();

    // Checking for messages coming from the protocol
    // can pick up a message/cycle for each virtual net
    for (int vnet = 0; vnet < inNode_ptr.size(); ++vnet) {
        MessageBuffer *b = inNode_ptr[vnet];
        if (b == nullptr) {
            continue;
        }

        if (b->isReady(curTime)) { // Is there a message waiting
            msg_ptr = b->peekMsgPtr();
            if (flitisizeMessage(msg_ptr, vnet)) {
                b->dequeue(curTime);
            }
        }
    }

    scheduleOutputLink();

    // Check if there are flits stalling a virtual channel. Track if a
    // message is enqueued to restrict ejection to one message per cycle.
    checkStallQueue();

    /*********** Check the incoming flit link **********/
    DPRINTF(RubyNetwork, "Number of input ports: %d\n", inPorts.size());
    for (auto &iPort: inPorts) {
        NetworkLink *inNetLink = iPort->inNetLink();
        if (inNetLink->isReady(curTick())) {
            flit *t_flit = inNetLink->consumeLink();
            DPRINTF(RubyNetwork, "Recieved flit:%s\n", *t_flit);
            assert(t_flit->m_width == iPort->bitWidth());

            //---------------------------------------------------------
            // RPM specifics.
            //---------------------------------------------------------
            // Change back to the original vnet to enque the message
            // to the corresponding message buffer.
            int vnet = t_flit->get_vnet();
            if (vnet >= (m_virtual_networks / 2)) {
                vnet = vnet - (m_virtual_networks / 2);
            }
            t_flit->set_dequeue_time(curTick());

            NDPRINTF(RubyNetworkPacket,
                    t_flit, m_net_ptr->get_trace_packet_id(),
                    "Router[%d] NI[%d] A flit is ejected %s\n",
                    t_flit->get_cur_router(), m_id, *t_flit);
            // If a tail flit is received, enqueue into the protocol buffers
            // if space is available. Otherwise, exchange non-tail flits for
            // credits.
            if (t_flit->get_type() == TAIL_ ||
                t_flit->get_type() == HEAD_TAIL_) {
                if (!iPort->messageEnqueuedThisCycle &&
                    outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
                    // Space is available. Enqueue to protocol buffer.
                    outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
                                               cyclesToTicks(Cycles(1)));

                    // Simply send a credit back since we are not buffering
                    // this flit in the NI
                    Credit *cFlit = new Credit(t_flit->get_vc(),
                                               true, curTick());
                    iPort->sendCredit(cFlit);
                    // Update stats and delete flit pointer
                    incrementStats(t_flit);
                    //---------------------------------------------------------
                    // RPM specifics.
                    //---------------------------------------------------------
                    m_rpm_net_ptr->remove_multicast_dest(
                            t_flit->get_packet_id(),
                            get_router_id(vnet));
                    delete t_flit;
                } else {
                    // No space available- Place tail flit in stall queue and
                    // set up a callback for when protocol buffer is dequeued.
                    // Stat update and flit pointer deletion will occur upon
                    // unstall.
                    iPort->m_stall_queue.push_back(t_flit);
                    m_stall_count[vnet]++;

                    outNode_ptr[vnet]->registerDequeueCallback([this]() {
                        dequeueCallback(); });
                }
            } else {
                // Non-tail flit. Send back a credit but not VC free signal.
                Credit *cFlit = new Credit(t_flit->get_vc(), false,
                                               curTick());
                // Simply send a credit back since we are not buffering
                // this flit in the NI
                iPort->sendCredit(cFlit);

                // Update stats and delete flit pointer.
                incrementStats(t_flit);
                delete t_flit;
            }
        }
    }

    /****************** Check the incoming credit link *******/

    for (auto &oPort: outPorts) {
        CreditLink *inCreditLink = oPort->inCreditLink();
        if (inCreditLink->isReady(curTick())) {
            Credit *t_credit = (Credit*) inCreditLink->consumeLink();
            outVcState[t_credit->get_vc()].increment_credit();
            if (t_credit->is_free_signal()) {
                outVcState[t_credit->get_vc()].setState(IDLE_,
                    curTick());
            }
            delete t_credit;
        }
    }


    // It is possible to enqueue multiple outgoing credit flits if a message
    // was unstalled in the same cycle as a new message arrives. In this
    // case, we should schedule another wakeup to ensure the credit is sent
    // back.
    for (auto &iPort: inPorts) {
        if (iPort->outCreditQueue()->getSize() > 0) {
            DPRINTF(RubyNetwork, "Sending a credit %s via %s at %ld\n",
            *(iPort->outCreditQueue()->peekTopFlit()),
            iPort->outCreditLink()->name(), clockEdge(Cycles(1)));
            iPort->outCreditLink()->
                scheduleEventAbsolute(clockEdge(Cycles(1)));
        }
    }
    checkReschedule();
}

void
RPMNetworkInterface::checkStallQueue()
{
    // Check all stall queues.
    // There is one stall queue for each input link
    for (auto &iPort: inPorts) {
        iPort->messageEnqueuedThisCycle = false;
        Tick curTime = clockEdge();

        if (!iPort->m_stall_queue.empty()) {
            for (auto stallIter = iPort->m_stall_queue.begin();
                 stallIter != iPort->m_stall_queue.end(); ) {
                flit *stallFlit = *stallIter;
                int vnet = stallFlit->get_vnet();

                // If we can now eject to the protocol buffer,
                // send back credits
                if (outNode_ptr[vnet]->areNSlotsAvailable(1,
                    curTime)) {
                    outNode_ptr[vnet]->enqueue(stallFlit->get_msg_ptr(),
                        curTime, cyclesToTicks(Cycles(1)));

                    // Send back a credit with free signal now that the
                    // VC is no longer stalled.
                    Credit *cFlit = new Credit(stallFlit->get_vc(), true,
                                                   curTick());
                    iPort->sendCredit(cFlit);

                    // Update Stats
                    incrementStats(stallFlit);

                    NDPRINTF(RubyNetworkPacket,
                            stallFlit, m_net_ptr->get_trace_packet_id(),
                            "Router[%d] NI[%d] A flit is ejected %s\n",
                            stallFlit->get_cur_router(), m_id, *stallFlit);

                    //---------------------------------------------------------
                    // RPM specifics.
                    //---------------------------------------------------------
                    m_rpm_net_ptr->remove_multicast_dest(
                            stallFlit->get_packet_id(),
                            get_router_id(vnet));

                    // Flit can now safely be deleted and removed from stall
                    // queue
                    delete stallFlit;
                    iPort->m_stall_queue.erase(stallIter);
                    m_stall_count[vnet]--;

                    // If there are no more stalled messages for this vnet, the
                    // callback on it's MessageBuffer is not needed.
                    if (m_stall_count[vnet] == 0)
                        outNode_ptr[vnet]->unregisterDequeueCallback();

                    iPort->messageEnqueuedThisCycle = true;
                    break;
                } else {
                    ++stallIter;
                }
            }
        }
    }
}

int
RPMNetworkInterface::calculateVC(int vnet)
{
    for (int i = 0; i < m_vc_per_vnet; i++) {
        int delta = m_vc_allocator[vnet];
        m_vc_allocator[vnet]++;
        if (m_vc_allocator[vnet] == m_vc_per_vnet)
            m_vc_allocator[vnet] = 0;

        if (outVcState[(vnet*m_vc_per_vnet) + delta].isInState(
                    IDLE_, curTick())) {
            vc_busy_counter[vnet] = 0;
            return ((vnet*m_vc_per_vnet) + delta);
        }
    }

    vc_busy_counter[vnet] += 1;
    //---------------------------------------------------------
    // RPM specifics.
    //---------------------------------------------------------
    panic_if(vc_busy_counter[vnet] > m_deadlock_threshold,
        "Router[%d] %s: Possible network deadlock "
        "in vnet: %d at time: %llu %s\n",
        get_router_id(vnet), name(), vnet,
        curTick(), m_rpm_net_ptr->get_multicast_status());

    return -1;
}

//---------------------------------------------------------
// RPM specifics.
//---------------------------------------------------------
bool
RPMNetworkInterface::flitisizeMessage(MsgPtr msg_ptr, int vnet)
{
    Message *net_msg_ptr = msg_ptr.get();
    NetDest net_msg_dest = net_msg_ptr->getDestination();

    // gets all the destinations associated with this message.
    std::vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

    // Number of flits is dependent on the link bandwidth available.
    // This is expressed in terms of bytes/cycle or the flit size
    OutputPort *oPort = getOutportForVnet(vnet);
    assert(oPort);
    int num_flits = (int)divCeil((float) m_rpm_net_ptr->MessageSizeType_to_int(
                net_msg_ptr->getMessageSize()), (float)oPort->bitWidth());

    DPRINTF(RubyNetwork, "Message Size:%d vnet:%d bitWidth:%d\n",
            m_rpm_net_ptr->MessageSizeType_to_int(
                net_msg_ptr->getMessageSize()),
            vnet, oPort->bitWidth());

    //-----------------------------------------------------------
    // Create a RouteInfo that uses the original vnet.
    //-----------------------------------------------------------
    RouteInfo route;
    route.vnet = vnet;
    route.net_dest = net_msg_dest;
    route.src_ni = m_id;
    route.src_router = oPort->routerID();
    route.dest_ni = -1;
    route.dest_router = -1;
    for (auto destID: dest_nodes) {
        route.dest_nis.insert(destID);
        route.dest_routers.insert(m_rpm_net_ptr->get_router_id(destID, vnet));
    }
    route.hops_traversed = -1;

    //-----------------------------------------------------------
    // Compute outports to see if there is a packet going to North
    //-----------------------------------------------------------
    Router* router = m_rpm_net_ptr->get_router(m_id, vnet);
    RPMRouter* rpmRouter = dynamic_cast<RPMRouter*>(router);
    assert(rpmRouter != nullptr);
    assert(router != NULL && rpmRouter->get_id() == oPort->routerID());

    // FIXME: m_id is not correct parameter
    // but it's not used in the function.
    outport2dests_t outport2dests =
        rpmRouter->route_compute(route, "Local");

    int outport = -1; // outport for non north direction.
    int vc = -1;
    int outport_north = -1;
    int vc_north = -1;
    int vnet_north = (m_virtual_networks / 2) + vnet;
    OutputPort *oPortNorth = getOutportForVnet(vnet_north);
    assert(oPortNorth);
    int num_flits_north = (int)divCeil(
      (float)m_rpm_net_ptr->MessageSizeType_to_int(
      net_msg_ptr->getMessageSize()),
      (float)oPortNorth->bitWidth());
    assert(m_virtual_networks % 2 == 0
            && vnet < (m_virtual_networks / 2));
    std::set<int> dest_routers_north;
    for (auto o2dests : outport2dests) {
      if (rpmRouter->isNorthPartition(o2dests.second)) {
        outport_north = o2dests.first;
        dest_routers_north = o2dests.second;
        vc_north = calculateVC(vnet_north);
      } else {
        outport = o2dests.first;
        vc = calculateVC(vnet);
      }
    }

    //-----------------------------------------------------------
    // Check both vc and vc_north.
    // There are three cases regarding number of packets to create.
    // 1) One packet going to N, NW, or NE (North direction).
    // 2) One packet going to non North direction.
    // 3) Two packets going to both direction.
    //-----------------------------------------------------------
    if (outport_north != -1 && outport == -1) {
        vc_north = calculateVC(vnet_north);
        if (vc_north == -1) return false;
    } else if (outport_north == -1 && outport != -1 ) {
        vc = calculateVC(vnet);
        if (vc == -1) return false;
    } else if (outport_north != -1 && outport != -1) {
        vc_north = calculateVC(vnet_north);
        vc = calculateVC(vnet);
        if (vc_north == -1 || vc == -1) return false;
    }

    // Create a route for North if there is an outport to north
    RouteInfo route_north = route; // Use data in route
    route_north.dest_nis.clear();
    route_north.dest_routers.clear();
    if (outport_north != -1) {
        std::set<int> dest_nis_north;
        assert(dest_routers_north.size() > 0);
        for (auto dest_ni_id : dest_nodes) {
            int dest_router_id
                = m_rpm_net_ptr->get_router_id(dest_ni_id, vnet);
            if (dest_routers_north.find(dest_router_id)
                    != dest_routers_north.end()) {
                dest_nis_north.insert(dest_ni_id);

                route.dest_nis.erase(dest_ni_id);
                route.dest_routers.erase(dest_router_id);

                route_north.dest_nis.insert(dest_ni_id);
                route_north.dest_routers.insert(dest_router_id);
            }
        }
        route.net_dest.removeDests(dest_nis_north);
        route_north.net_dest.removeDestsExcept(dest_nis_north);
        m_rpm_net_ptr->increment_injected_packets(vnet_north);
    }
    m_rpm_net_ptr->increment_injected_packets(vnet);

    //-----------------------------------------------------------
    // Create flits that going to non-north outpots.
    //-----------------------------------------------------------
    if (route.dest_routers.size() > 0) {
        int packet_id = m_rpm_net_ptr->get_next_packet_id();
        m_rpm_net_ptr->add_multicast_packet(
                packet_id, curCycle(), route.dest_routers);
        // FIXME
        //m_rpm_net_ptr->update_traffic_distribution(route);
        MsgPtr new_msg_ptr = msg_ptr->clone();
        for (int i = 0; i < num_flits; i++) {
            m_rpm_net_ptr->increment_injected_flits(vnet);
            flit *fl = new RPMFlit(packet_id,
                    i, vc, vnet, route, num_flits, new_msg_ptr,
                    m_rpm_net_ptr->MessageSizeType_to_int(
                        net_msg_ptr->getMessageSize()),
                    oPort->bitWidth(), curTick(), oPort->routerID());

            fl->set_src_delay(curTick() - msg_ptr->getTime());
            niOutVcs[vc].insert(fl);
            NDPRINTF(RubyNetworkPacket, fl, m_net_ptr->get_trace_packet_id(),
                    "Router[%d] NI[%d] A flit is created %s\n",
                    fl->get_cur_router(), m_id, *fl);
        }
        m_ni_out_vcs_enqueue_time[vc] = curTick();
        outVcState[vc].setState(ACTIVE_, curTick());
    }

    //-----------------------------------------------------------
    // Create flits that going to north outports.
    //-----------------------------------------------------------
    if (outport_north != -1) {
        int packet_id = m_rpm_net_ptr->get_next_packet_id();
        m_rpm_net_ptr->add_multicast_packet(
                packet_id, curCycle(), route_north.dest_routers);
        // FIXME
        //m_rpm_net_ptr->update_traffic_distribution(route_north);
        MsgPtr new_msg_ptr = msg_ptr->clone();
        for (int i = 0; i < num_flits_north; i++) {
            m_rpm_net_ptr->increment_injected_flits(vnet);
            flit *fl = new RPMFlit(packet_id,
                    i, vc_north, vnet_north,
                    route_north, num_flits_north, new_msg_ptr,
                    m_rpm_net_ptr->MessageSizeType_to_int(
                        net_msg_ptr->getMessageSize()),
                    oPortNorth->bitWidth(), curTick(), oPortNorth->routerID());

            fl->set_src_delay(curTick() - msg_ptr->getTime());
            niOutVcs[vc_north].insert(fl);
            NDPRINTF(RubyNetworkPacket, fl, m_net_ptr->get_trace_packet_id(),
                    "Router[%d] NI[%d] A flit is created %s\n",
                    fl->get_cur_router(), m_id, *fl);
        }
        m_ni_out_vcs_enqueue_time[vc_north] = curTick();
        outVcState[vc_north].setState(ACTIVE_, curTick());
    }

    return true ;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
