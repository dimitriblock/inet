//
// Copyright (C) 2015 Andras Varga
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
// Author: Andras Varga
//

#include "DcfUpperMac.h"
#include "UpperMacContext.h"
#include "Ieee80211NewMac.h"
#include "IRx.h"
#include "IContentionTx.h"
#include "IImmediateTx.h"
#include "IMacQoSClassifier.h"
#include "MacUtils.h"
#include "MacParameters.h"
#include "FrameExchanges.h"
#include "inet/common/queue/IPassiveQueue.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

Define_Module(DcfUpperMac);

DcfUpperMac::DcfUpperMac()
{
}

DcfUpperMac::~DcfUpperMac()
{
    delete frameExchange;
    while (!transmissionQueue.empty()) {
        Ieee80211Frame *temp = transmissionQueue.front();
        transmissionQueue.pop_front();
        delete temp;
    }
}

void DcfUpperMac::initialize()
{
    mac = check_and_cast<Ieee80211NewMac *>(getModuleByPath(par("macModule")));
    rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
    immediateTx = check_and_cast<IImmediateTx *>(getModuleByPath(par("immediateTxModule")));
    contentionTx = nullptr;
    collectContentionTxModules(getModuleByPath(par("firstContentionTxModule")), contentionTx);

    maxQueueSize = mac->par("maxQueueSize");  //FIXME

    readParameters();
    utils = new MacUtils(params);
    rx->setAddress(params->getAddress());

    WATCH(maxQueueSize);
    WATCH(fragmentationThreshold);
    WATCH(sequenceNumber);
    WATCH_LIST(transmissionQueue);
}

inline double fallback(double a, double b) {return a!=-1 ? a : b;}
inline simtime_t fallback(simtime_t a, simtime_t b) {return a!=-1 ? a : b;}

void DcfUpperMac::readParameters()
{
    MacParameters *params = new MacParameters();

    const Ieee80211ModeSet *modeSet = Ieee80211ModeSet::getModeSet(*par("opMode").stringValue());
    double bitrate = par("bitrate");
    const IIeee80211Mode *dataFrameMode = (bitrate == -1) ? modeSet->getFastestMode() : modeSet->getMode(bps(bitrate));
    const IIeee80211Mode *basicFrameMode = modeSet->getSlowestMode();

    params->setAddress(mac->getAddress());
    params->setBasicFrameMode(basicFrameMode);
    params->setDefaultDataFrameMode(dataFrameMode);
    params->setShortRetryLimit(fallback(par("shortRetryLimit"), 7));
    params->setRtsThreshold(par("rtsThreshold"));

    params->setEdcaEnabled(false);
    params->setSlotTime(fallback(par("slotTime"), dataFrameMode->getSlotTime()));
    params->setSifsTime(fallback(par("sifsTime"), dataFrameMode->getSifsTime()));
    params->setDifsTime(fallback(par("difsTime"), dataFrameMode->getSifsTime() + dataFrameMode->getAifsNumber(AC_LEGACY)*params->getSlotTime()));
    params->setEifsTime(AC_LEGACY, params->getSifsTime() + params->getAifsTime(AC_LEGACY) + basicFrameMode->getDuration(LENGTH_ACK));
    params->setCwMin(AC_LEGACY, fallback(par("cwMin"), dataFrameMode->getCwMin(AC_LEGACY)));
    params->setCwMax(AC_LEGACY, fallback(par("cwMax"), dataFrameMode->getCwMax(AC_LEGACY)));
    params->setCwMulticast(AC_LEGACY, fallback(par("cwMulticast"), dataFrameMode->getCwMin(AC_LEGACY)));

    this->params = params;
}

void DcfUpperMac::handleMessage(cMessage *msg)
{
    if (msg->getContextPointer() != nullptr)
        ((MacPlugin *)msg->getContextPointer())->handleSelfMessage(msg);
    else
        ASSERT(false);
}

void DcfUpperMac::upperFrameReceived(Ieee80211DataOrMgmtFrame *frame)
{
    Enter_Method("upperFrameReceived(\"%s\")", frame->getName());
    take(frame);

    // check for queue overflow
    if (maxQueueSize && transmissionQueue.size() == maxQueueSize) {
        EV << "message " << frame << " received from higher layer but MAC queue is full, dropping message\n";
        delete frame;
        return;
    }

    // must be a Ieee80211DataOrMgmtFrame, within the max size because we don't support fragmentation
    if (frame->getByteLength() > fragmentationThreshold)
        throw cRuntimeError("message from higher layer (%s)%s is too long for 802.11b, %d bytes (fragmentation is not supported yet)",
                frame->getClassName(), frame->getName(), (int)(frame->getByteLength()));
    EV_INFO << "Frame " << frame << " received from higher layer, receiver = " << frame->getReceiverAddress() << endl;
    ASSERT(!frame->getReceiverAddress().isUnspecified());

    // fill in missing fields (receiver address, seq number), and insert into the queue
    frame->setTransmitterAddress(params->getAddress());
    frame->setSequenceNumber(sequenceNumber);
    sequenceNumber = (sequenceNumber + 1) % 4096;    //XXX seqNum must be checked upon reception of frames!
    if (frameExchange)
        transmissionQueue.push_back(frame);
    else {
        startSendDataFrameExchange(frame, 0, AC_LEGACY);
    }
}

void DcfUpperMac::lowerFrameReceived(Ieee80211Frame *frame)
{
    Enter_Method("lowerFrameReceived(\"%s\")", frame->getName());
    delete frame->removeControlInfo();          //TODO
    take(frame);

    if (utils->isForUs(frame)) {
        if (Ieee80211RTSFrame *rtsFrame = dynamic_cast<Ieee80211RTSFrame *>(frame)) {
            sendCts(rtsFrame);
        }
        else if (Ieee80211DataOrMgmtFrame *dataOrMgmtFrame = dynamic_cast<Ieee80211DataOrMgmtFrame *>(frame)) {
            if (!utils->isBroadcast(frame) && !utils->isMulticast(frame))
                sendAck(dataOrMgmtFrame);
            mac->sendUp(dataOrMgmtFrame);
        }
        else {
            // offer frame to ongoing frame exchange
            bool processed = frameExchange ? frameExchange->lowerFrameReceived(frame) : false;
            if (!processed) {
                EV_INFO << "Unexpected frame " << frame->getName() << ", dropping\n";
                delete frame;
            }
        }
    }
    else {
        EV_INFO << "This frame is not for us" << std::endl;
        delete frame;
    }
}

void DcfUpperMac::transmissionComplete(ITxCallback *callback, int txIndex)
{
    Enter_Method("transmissionComplete()");
    if (callback)
        callback->transmissionComplete(txIndex);
}

void DcfUpperMac::internalCollision(ITxCallback *callback, int txIndex)
{
    Enter_Method("transmissionComplete()");
    if (callback)
        callback->internalCollision(txIndex);
}

void DcfUpperMac::startSendDataFrameExchange(Ieee80211DataOrMgmtFrame *frame, int txIndex, AccessCategory ac)
{
    ASSERT(!frameExchange);

    if (utils->isBroadcast(frame) || utils->isMulticast(frame))
        utils->setFrameMode(frame, params->getBasicFrameMode());
    else
        utils->setFrameMode(frame, params->getDefaultDataFrameMode());

    UpperMacContext context;
    context.ownerModule = this;
    context.params = params;
    context.utils = utils;
    context.contentionTx = contentionTx;
    context.immediateTx = immediateTx;

    bool useRtsCts = frame->getByteLength() > params->getRtsThreshold();
    if (utils->isBroadcast(frame) || utils->isMulticast(frame))
        frameExchange = new SendMulticastDataFrameExchange(&context, this, frame, txIndex, ac);
    else if (useRtsCts)
        frameExchange = new SendDataWithRtsCtsFrameExchange(&context, this, frame, txIndex, ac);
    else
        frameExchange = new SendDataWithAckFrameExchange(&context, this, frame, txIndex, ac);

    frameExchange->start();
}

void DcfUpperMac::frameExchangeFinished(IFrameExchange *what, bool successful)
{
    EV_INFO << "Frame exchange finished" << std::endl;
    delete frameExchange;
    frameExchange = nullptr;

    if (!transmissionQueue.empty()) {
        Ieee80211DataOrMgmtFrame *frame = check_and_cast<Ieee80211DataOrMgmtFrame *>(transmissionQueue.front());
        transmissionQueue.pop_front();
        startSendDataFrameExchange(frame, 0, AC_LEGACY);
    }
}

void DcfUpperMac::sendAck(Ieee80211DataOrMgmtFrame *frame)
{
    Ieee80211ACKFrame *ackFrame = utils->buildAckFrame(frame);
    immediateTx->transmitImmediateFrame(ackFrame, params->getSifsTime(), nullptr);
}

void DcfUpperMac::sendCts(Ieee80211RTSFrame *frame)
{
    Ieee80211CTSFrame *ctsFrame = utils->buildCtsFrame(frame);
    immediateTx->transmitImmediateFrame(ctsFrame, params->getSifsTime(), nullptr);
}

} // namespace ieee80211
} // namespace inet
