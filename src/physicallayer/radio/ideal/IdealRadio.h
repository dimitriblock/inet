//
// Copyright (C) 2013 OpenSim Ltd
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//
// author: Zoltan Bojthe
//


#ifndef __INET_IDEALRADIO_H
#define __INET_IDEALRADIO_H


#include "INETDefs.h"

#include "IdealRadioFrame_m.h"
#include "IdealRadioChannelAccess.h"
#include "ILifecycle.h"
#include "RadioState.h"


/**
 * This module implements a full-duplex, collision-free and interference-free radio.
 * It should be used together with IdealWirelessMac.
 *
 * See the NED file for details.
 */
class INET_API IdealRadio : public IdealRadioChannelAccess, public ILifecycle
{
  public:
    IdealRadio();
    virtual ~IdealRadio();

    /** Returns the current transmission range */
    virtual int getTransmissionRange() const { return transmissionRange; }

    bool isEnabled() const { return rs != RadioState::OFF && rs != RadioState::SLEEP; }

  protected:
    virtual int numInitStages() const { return NUM_INIT_STAGES; }
    virtual void initialize(int stage);
    virtual void finish();

    virtual void handleMessage(cMessage *msg);

    virtual void handleUpperMsg(cMessage *msg);

    virtual void handleSelfMsg(cMessage *msg);

    virtual void handleCommand(cMessage *msg);

    virtual void handleLowerMsgStart(IdealRadioFrame *radioFrame);

    virtual void handleLowerMsgEnd(IdealRadioFrame *radioFrame);

    /** Sends a message to the upper layer */
    virtual void sendUp(IdealRadioFrame *radioFrame);

    /** Sends a message to the channel */
    virtual void sendDown(IdealRadioFrame *radioFrame);

    /** Encapsulates a MAC frame into a radio frame */
    virtual IdealRadioFrame *encapsulatePacket(cPacket *msg);

    /** Updates the radio state, and also sends a radioState signal */
    virtual void updateRadioState();

    /** Create a new IdealRadioFrame */
    virtual IdealRadioFrame *createRadioFrame() { return new IdealRadioFrame(); }

    virtual void setRadioState(RadioState::State newState);

    virtual void updateDisplayString();

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj);

    // ILifecycle:
    virtual bool handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback);

  protected:
    typedef std::list<cMessage *> RecvBuff;
    RecvBuff recvBuff;

    /** Parameters */
    //@{
    double transmissionRange;           // [meter]
    double bitrate;                     // [bps]
    bool drawCoverage;                  // if true, draw coverage circles
    //@}

    /** @name Gate Ids */
    //@{
    int upperLayerOutGateId;
    int upperLayerInGateId;
    int radioInGateId;
    //@}

    /** Radio State and helper variables */
    //@{
    int concurrentReceives;    // number of current receives
    bool inTransmit;           // is in transmit mode
    RadioState::State rs;
    //@}

    // signals:
    static simsignal_t radioStateSignal; // signaling RadioState::State enum when state changed
};

#endif

