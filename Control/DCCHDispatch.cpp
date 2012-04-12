/**@file Idle-mode dispatcher for dedicated control channels. */

/*
* Copyright 2008, 2009, 2011 Free Software Foundation, Inc.
* Copyright 2011 Range Networks, Inc.
*
* This software is distributed under the terms of the GNU Affero Public License.
* See the COPYING file in the main directory for details.
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/




#include "ControlCommon.h"
#include "TransactionTable.h"
#include "RadioResource.h"
#include "MobilityManagement.h"
#include <GSMLogicalChannel.h>
#include <GSML3MMMessages.h>
#include <GSML3RRMessages.h>
#include <SIPUtility.h>
#include <SIPInterface.h>
#include <GSMConfig.h>
#include <gsmL1prim.h>

#include <Logger.h>
#undef WARNING
#include <Reporting.h>
#include <Globals.h>

#define MAX_UDP_LENGTH 1500

using namespace std;
using namespace GSM;
using namespace Control;

// TODO: We should take ports and IP from config.
UDPSocket RLCMACSocket(5934, "127.0.0.1", 5944);

/**
	Dispatch the appropriate controller for a Mobility Management message.
	@param req A pointer to the initial message.
	@param DCCH A pointer to the logical channel for the transaction.
*/
void DCCHDispatchMM(const L3MMMessage* req, LogicalChannel *DCCH)
{
	assert(req);
	L3MMMessage::MessageType MTI = (L3MMMessage::MessageType)req->MTI();
	switch (MTI) {
		case L3MMMessage::LocationUpdatingRequest:
			LocationUpdatingController(dynamic_cast<const L3LocationUpdatingRequest*>(req),DCCH);
			break;
		case L3MMMessage::IMSIDetachIndication:
			IMSIDetachController(dynamic_cast<const L3IMSIDetachIndication*>(req),DCCH);
			break;
		case L3MMMessage::CMServiceRequest:
			CMServiceResponder(dynamic_cast<const L3CMServiceRequest*>(req),DCCH);
			break;
		default:
			LOG(NOTICE) << "unhandled MM message " << MTI << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}


/**
	Dispatch the appropriate controller for a Radio Resource message.
	@param req A pointer to the initial message.
	@param DCCH A pointer to the logical channel for the transaction.
*/
void DCCHDispatchRR(const L3RRMessage* req, LogicalChannel *DCCH)
{
	LOG(DEBUG) << "checking MTI"<< (L3RRMessage::MessageType)req->MTI();

	// TODO SMS -- This needs to handle SACCH Measurement Reports.

	assert(req);
	L3RRMessage::MessageType MTI = (L3RRMessage::MessageType)req->MTI();
	switch (MTI) {
		case L3RRMessage::PagingResponse:
			PagingResponseHandler(dynamic_cast<const L3PagingResponse*>(req),DCCH);
			break;
		case L3RRMessage::AssignmentComplete:
			AssignmentCompleteHandler(dynamic_cast<const L3AssignmentComplete*>(req),
										dynamic_cast<TCHFACCHLogicalChannel*>(DCCH));
			break;
		default:
			LOG(NOTICE) << "unhandled RR message " << MTI << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}


void DCCHDispatchMessage(const L3Message* msg, LogicalChannel* DCCH)
{
	// Each protocol has it's own sub-dispatcher.
	switch (msg->PD()) {
		case L3MobilityManagementPD:
			DCCHDispatchMM(dynamic_cast<const L3MMMessage*>(msg),DCCH);
			break;
		case L3RadioResourcePD:
			DCCHDispatchRR(dynamic_cast<const L3RRMessage*>(msg),DCCH);
			break;
		default:
			LOG(NOTICE) << "unhandled protocol " << msg->PD() << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}




/** Example of a closed-loop, persistent-thread control function for the DCCH. */
void Control::DCCHDispatcher(LogicalChannel *DCCH)
{
	while (1) {
		try {
			// Wait for a transaction to start.
			LOG(DEBUG) << "waiting for " << *DCCH << " ESTABLISH";
			DCCH->waitForPrimitive(ESTABLISH);
			// Pull the first message and dispatch a new transaction.
			gReports.incr("OpenBTS.GSM.RR.ChannelSiezed");
			const L3Message *message = getMessage(DCCH);
			LOG(DEBUG) << *DCCH << " received " << *message;
			DCCHDispatchMessage(message,DCCH);
			delete message;
		}

		// Catch the various error cases.

		catch (ChannelReadTimeout except) {
			LOG(NOTICE) << "ChannelReadTimeout";
			// Cause 0x03 means "abnormal release, timer expired".
			DCCH->send(L3ChannelRelease(0x03));
			gTransactionTable.remove(except.transactionID());
		}
		catch (UnexpectedPrimitive except) {
			LOG(NOTICE) << "UnexpectedPrimitive";
			// Cause 0x62 means "message type not not compatible with protocol state".
			DCCH->send(L3ChannelRelease(0x62));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (UnexpectedMessage except) {
			LOG(NOTICE) << "UnexpectedMessage";
			// Cause 0x62 means "message type not not compatible with protocol state".
			DCCH->send(L3ChannelRelease(0x62));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (UnsupportedMessage except) {
			LOG(NOTICE) << "UnsupportedMessage";
			// Cause 0x61 means "message type not implemented".
			DCCH->send(L3ChannelRelease(0x61));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (Q931TimerExpired except) {
			LOG(NOTICE) << "Q.931 T3xx timer expired";
			// Cause 0x03 means "abnormal release, timer expired".
			// TODO -- Send diagnostics.
			DCCH->send(L3ChannelRelease(0x03));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (SIP::SIPTimeout except) {
			// FIXME -- The transaction ID should be an argument here.
			LOG(WARNING) << "Uncaught SIPTimeout, will leave a stray transcation";
			// Cause 0x03 means "abnormal release, timer expired".
			DCCH->send(L3ChannelRelease(0x03));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (SIP::SIPError except) {
			// FIXME -- The transaction ID should be an argument here.
			LOG(WARNING) << "Uncaught SIPError, will leave a stray transcation";
			// Cause 0x01 means "abnormal release, unspecified".
			DCCH->send(L3ChannelRelease(0x01));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
	}
}

/** Example of a closed-loop, persistent-thread control function for the PDCH. */
void Control::PDCHDispatcher(LogicalChannel *PDCH)
{
	const RLCMACFrame *frame = NULL;
	while (1) {
		frame = ((PDTCHLogicalChannel*)PDCH)->recvPDCH();
		if (!frame) { 
			delete frame;
			frame = NULL;
			continue;
 		}
		LOG(NOTICE) << " PDCH received frame " << *frame;
		txPhDataInd(frame);
		delete frame;
		frame = NULL;
	}
}

void Control::txPhReadyToSendInd(unsigned Tn, int Fn)
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct GsmL1_Prim_t *prim = (struct GsmL1_Prim_t *)buffer;

	prim->id = GsmL1_PrimId_PhReadyToSendInd;
	prim->u.phReadyToSendInd.u8Tn = Tn;
	prim->u.phReadyToSendInd.u32Fn = Fn;
	prim->u.phReadyToSendInd.sapi = GsmL1_Sapi_Pdtch;
	ofs = sizeof(*prim);

	RLCMACSocket.write(buffer, ofs);
}

void Control::txPhDataInd(const RLCMACFrame *frame)
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct GsmL1_Prim_t *prim = (struct GsmL1_Prim_t *)buffer;

	prim->id = GsmL1_PrimId_PhDataInd;
	frame->pack((unsigned char*)&(prim->u.phDataInd.msgUnitParam.u8Buffer[ofs]));
	ofs += frame->size() >> 3;
	prim->u.phDataInd.msgUnitParam.u8Size = ofs;
	prim->u.phDataInd.sapi = GsmL1_Sapi_Pdtch;
	ofs = sizeof(*prim);

	COUT("TX: [ BTS -> PCU ] PhDataInd:" << *frame);
	RLCMACSocket.write(buffer, ofs);
}

int readL1Prim(unsigned char* buffer, RLCMACFrame *frame)
{
	int rc = 0;
	size_t readIndex = 8;
	struct GsmL1_Prim_t *prim = (struct GsmL1_Prim_t *)buffer;


	switch(prim->id) 
	case GsmL1_PrimId_PhDataReq:
	{
		rc = 1;
		frame->unpack((const unsigned char*)prim->u.phDataReq.msgUnitParam.u8Buffer);
	}

	if( (prim->u.phDataReq.msgUnitParam.u8Buffer[0]!= 0x41)&&(prim->u.phDataReq.msgUnitParam.u8Buffer[1]!= 0x94))
	{
		COUT("RX: [ BTS <- PCU ] PhDataReq:" << *frame);
	}		

	return rc;
}

void Control::GPRSReader(LogicalChannel *PDCH)
{
	RLCMACSocket.nonblocking();
	char buf[MAX_UDP_LENGTH];
	unsigned char targ[23];

	while (1)
	{
		int count = RLCMACSocket.read(buf, 3000);
		if (count>0)
		{
			RLCMACFrame *frame = new RLCMACFrame(23*8);
			if (!readL1Prim((unsigned char*) buf, frame))
			{
				delete frame;
				continue;
			}
			if (frame->payloadType() != 0x03) // RLCMACReserved
			{
				((PDTCHLogicalChannel*)PDCH)->sendRLCMAC(frame);
			}
			else
			{
				COUT(" GPRS downlink assignment CCCH");
				// Get an AGCH to send on.
				CCCHLogicalChannel *AGCH = gBTS.getAGCH();
				// Someone had better have created a least one AGCH.
				assert(AGCH);
				// Check AGCH load now.
				if (AGCH->load()>gConfig.getNum("GSM.CCCH.AGCH.QMax"))
				{
					COUT(" GPRS AGCH congestion");
					return;
				}
				unsigned RA = 0x7a;
				const GSM::Time when = gBTS.time();
				GSM::Time start = gBTS.time();
				LogicalChannel *LCH = NULL;
				LCH = PDCH;
				bool gprs = true;
				frame->pack(targ);
				// Assignment, GSM 04.08 3.3.1.1.3.1.
				// Create the ImmediateAssignment message.
				int initialTA = 0;
				const L3ImmediateAssignment assign(
					gprs,
					L3RequestReference(RA,when),
					LCH->channelDescription(),
					start, // We use it for TBF starting time.
					// This message assigns a downlink TBF to the mobile station identified in the IA Rest Octets IE
					L3DedicatedModeOrTBF(1,0,1),
					L3TimingAdvance(initialTA),
					targ
				);
				COUT("sending " << assign);
				AGCH->send(assign);
				delete frame;
			}
		}
	}
}

// vim: ts=4 sw=4
