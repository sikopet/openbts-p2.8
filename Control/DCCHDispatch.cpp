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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <Logger.h>
#undef WARNING

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

void Control::txPhConnectInd()
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	prim->msg_type = PCU_IF_MSG_INFO_IND;
	prim->bts_nr = 1;
	
	prim->u.info_ind.flags |= PCU_IF_FLAG_ACTIVE;
	prim->u.info_ind.flags |= PCU_IF_FLAG_CS1;
	prim->u.info_ind.trx[0].arfcn  = gConfig.getNum("GSM.Radio.C0");
	prim->u.info_ind.trx[0].pdch_mask = 0x80;
	prim->u.info_ind.trx[0].tsc[7] = gConfig.getNum("GSM.Identity.BSIC.BCC");
	
	prim->u.info_ind.initial_cs = gConfig.getNum("GPRS.INITIAL_CS");

	prim->u.info_ind.t3142 = gConfig.getNum("GPRS.T3142");
	prim->u.info_ind.t3169 = gConfig.getNum("GPRS.T3169");
	prim->u.info_ind.t3191 = gConfig.getNum("GPRS.T3191");
	prim->u.info_ind.t3193_10ms = gConfig.getNum("GPRS.T3193_10MS");
	prim->u.info_ind.t3195 = gConfig.getNum("GPRS.T3195");
	prim->u.info_ind.n3101 = gConfig.getNum("GPRS.T3101");
	prim->u.info_ind.n3103 = gConfig.getNum("GPRS.T3103");
	prim->u.info_ind.n3105 = gConfig.getNum("GPRS.T3105");
	
	/* RAI */
	prim->u.info_ind.bsic = gConfig.getNum("GSM.Identity.BSIC.BCC");
	prim->u.info_ind.mcc = gConfig.getNum("GPRS.MCC");
	prim->u.info_ind.mnc = gConfig.getNum("GPRS.MNC");
	prim->u.info_ind.lac = gConfig.getNum("GSM.Identity.LAC");
	prim->u.info_ind.rac = gConfig.getNum("GPRS.RAC");
	/* NSE */
	prim->u.info_ind.nsei = gConfig.getNum("GPRS.NSEI");
	/* cell  */
	prim->u.info_ind.cell_id = gConfig.getNum("GPRS.CELL_ID");
	prim->u.info_ind.repeat_time = gConfig.getNum("GPRS.REPEAT_TIME");
	prim->u.info_ind.repeat_count = gConfig.getNum("GPRS.REPEAT_COUNT");
	prim->u.info_ind.bvci = gConfig.getNum("GPRS.BVCI");

	prim->u.info_ind.cv_countdown = gConfig.getNum("GPRS.CV_COUNTDOWN");
	/* NSVC */
	prim->u.info_ind.nsvci[0] = gConfig.getNum("GPRS.NSVCI");
	prim->u.info_ind.local_port[0] = gConfig.getNum("GPRS.NSVC_LPORT");
	prim->u.info_ind.remote_port[0] = gConfig.getNum("GPRS.NSVC_RPORT");
	
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_port = htons(gConfig.getNum("GPRS.NSVC_RPORT"));
	inet_aton("127.0.0.1", &dest.sin_addr);
	prim->u.info_ind.remote_ip[0] = ntohl(dest.sin_addr.s_addr);

	ofs = sizeof(*prim);

	COUT("TX: [ BTS -> PCU ] PhConnectInd: ARFCN: " << gConfig.getNum("GSM.Radio.C0")
		<<" TN: " << gConfig.getNum("GPRS.TS") << " TSC: " << gConfig.getNum("GSM.Identity.BSIC.BCC"));
	RLCMACSocket.write(buffer, ofs);
}

void Control::txPhRaInd(unsigned ra, int Fn, unsigned ta)
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	prim->msg_type = PCU_IF_MSG_RACH_IND;
	prim->bts_nr = 1;
	prim->u.rach_ind.sapi = PCU_IF_SAPI_RACH;
	prim->u.rach_ind.ra = ra;
	prim->u.rach_ind.qta = ta;
	prim->u.rach_ind.fn = Fn;
	prim->u.rach_ind.arfcn = gConfig.getNum("GSM.Radio.C0");
	ofs = sizeof(*prim);

	COUT("TX: [ BTS -> PCU ] PhRaInd: RA: " << ra <<" FN: " << Fn << " TA: " << ta);
	RLCMACSocket.write(buffer, ofs);
}

void Control::txPhReadyToSendInd(unsigned Tn, int Fn)
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	prim->msg_type = PCU_IF_MSG_RTS_REQ;
	prim->bts_nr = 1;
	prim->u.rts_req.sapi = PCU_IF_SAPI_PDTCH;
	prim->u.rts_req.fn = Fn;
	prim->u.rts_req.arfcn = gConfig.getNum("GSM.Radio.C0");
	prim->u.rts_req.trx_nr = 0;
	prim->u.rts_req.ts_nr = Tn;
	int bn = 0;

	int fn52 = 4;
	while(fn52 < 52)
	{
		if ((Fn%52)<fn52)
		{
			break;
		}
		else
		{
			bn++;
			fn52 += 4;
		}
	}

	prim->u.rts_req.block_nr = bn;

	ofs = sizeof(*prim);

	RLCMACSocket.write(buffer, ofs);
	txMphTimeInd();
}

void Control::txMphTimeInd()
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	prim->msg_type = PCU_IF_MSG_TIME_IND;
	prim->bts_nr = 1;
	prim->u.time_ind.fn = gBTS.time().FN();
	ofs = sizeof(*prim);

	RLCMACSocket.write(buffer, ofs);
}

void Control::txPhDataInd(const RLCMACFrame *frame, GSM::Time readTime)
{
	char buffer[MAX_UDP_LENGTH];
	int ofs = 0;
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	prim->msg_type = PCU_IF_MSG_DATA_IND;
	prim->bts_nr = 1;
	
	prim->u.data_ind.sapi = PCU_IF_SAPI_PDTCH;
	frame->pack((unsigned char*)&(prim->u.data_ind.data[ofs]));
	ofs += frame->size() >> 3;
	prim->u.data_ind.len = ofs;
	prim->u.data_ind.arfcn = gConfig.getNum("GSM.Radio.C0");
	int Fn = readTime.FN();
	prim->u.data_ind.fn = Fn;
	int bn = 0;

	int fn52 = 4;
	while(fn52 < 52)
	{
		if ((Fn%52)<fn52)
		{
			break;
		}
		else
		{
			bn++;
			fn52 += 4;
		}
	}

	prim->u.data_ind.block_nr = bn;
	prim->u.data_ind.trx_nr = 0;
	prim->u.data_ind.ts_nr = gConfig.getNum("GPRS.TS");

	ofs = sizeof(*prim);

	COUT("TX: [ BTS -> PCU ] PhDataInd:" << *frame);
	RLCMACSocket.write(buffer, ofs);
	txMphTimeInd();
}


BitVector* readL1Prim(unsigned char* buffer, int *sapi)
{
	struct gsm_pcu_if *prim = (struct gsm_pcu_if *)buffer;

	switch(prim->msg_type)
	case PCU_IF_MSG_DATA_REQ:
	{
		*sapi = prim->u.data_req.sapi;
		BitVector * msg = new BitVector(prim->u.data_req.len*8);
		msg->unpack((const unsigned char*)prim->u.data_req.data);
		return msg;
	}

	return NULL;
}

void Control::GPRSReader(LogicalChannel *PDCH)
{
	RLCMACSocket.nonblocking();

	char buf[MAX_UDP_LENGTH];

	// Send to PCU PhConnectInd primitive.
	
	txPhConnectInd();

	while (1)
	{
		int count = RLCMACSocket.read(buf, 3000);
		if (count>0)
		{
			int sapi;
			BitVector *msg = readL1Prim((unsigned char*) buf, &sapi);
			if (!msg)
			{
				delete msg;
				continue;
			}
			if (sapi == PCU_IF_SAPI_PDTCH)
			{
				RLCMACFrame *frame = new RLCMACFrame(*msg);
				((PDTCHLogicalChannel*)PDCH)->sendRLCMAC(frame);
			}
			else if (sapi == PCU_IF_SAPI_AGCH)
			{
				// Get an AGCH to send on.
				CCCHLogicalChannel *AGCH = gBTS.getAGCH();
				assert(AGCH);
				// Check AGCH load now.
				if (AGCH->load()>gConfig.getNum("GSM.CCCH.AGCH.QMax"))
				{
					COUT(" GPRS AGCH congestion");
					return;
				}
				L3Frame *l3 = new L3Frame(msg->tail(8), UNIT_DATA);
				COUT("RX: [ BTS <- PCU ] AGCH: " << *l3);
				AGCH->send(l3);
			}
			else if (sapi == PCU_IF_SAPI_PCH)
			{
				// Get an PCH to send on.
				CCCHLogicalChannel *PCH = gBTS.getPCH();
				assert(PCH);
				L3Frame *l3 = new L3Frame(msg->tail(8*4), UNIT_DATA);
				COUT("RX: [ BTS <- PCU ] PCH: " << *l3);
				PCH->send(l3);
			}
			delete msg;
		}
	}
}

// vim: ts=4 sw=4
