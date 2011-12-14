/* RLCMACTest.cpp
 *
 * Copyright (C) 2011 Ivan Klyuchnikov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */



#include "BitVector.h"
#include <iostream>
#include <cstdlib>
#include "csn1.h"
#include "gsm_rlcmac.h"
using namespace std;


int main(int argc, char *argv[])
{

	/*
	cout << "sizeof RlcMacUplink                         " << sizeof(RlcMacUplink_t) << endl;
	cout << "sizeof Packet_Cell_Change_Failure_t         " << sizeof(Packet_Cell_Change_Failure_t) << endl;
	cout << "sizeof Packet_Control_Acknowledgement_t     " << sizeof(Packet_Control_Acknowledgement_t) << endl;
	cout << "sizeof Packet_Downlink_Ack_Nack_t           " << sizeof(Packet_Downlink_Ack_Nack_t) << endl;
	cout << "sizeof EGPRS_PD_AckNack_t		     " << sizeof(EGPRS_PD_AckNack_t) << endl;
	cout << "sizeof Packet_Uplink_Dummy_Control_Block_t  " << sizeof(Packet_Uplink_Dummy_Control_Block_t) << endl;
	cout << "sizeof Packet_Measurement_Report_t          " << sizeof(Packet_Measurement_Report_t) << endl;
	cout << "sizeof Packet_Resource_Request_t            " << sizeof(Packet_Resource_Request_t) << endl;
	cout << "sizeof Packet_Mobile_TBF_Status_t           " << sizeof(Packet_Mobile_TBF_Status_t) << endl;
	cout << "sizeof Packet_PSI_Status_t                  " << sizeof(Packet_PSI_Status_t) << endl;
	cout << "sizeof Packet_Enh_Measurement_Report_t      " << sizeof(Packet_Enh_Measurement_Report_t) << endl;
	cout << "sizeof Packet_Cell_Change_Notification_t    " << sizeof(Packet_Cell_Change_Notification_t) << endl;
	cout << "sizeof Packet_SI_Status_t                   " << sizeof(Packet_SI_Status_t) << endl;
	cout << "sizeof Additional_MS_Rad_Access_Cap_t       " << sizeof(Additional_MS_Rad_Access_Cap_t) << endl;
	cout << "sizeof Packet_Pause_t                       " << sizeof(Packet_Pause_t) << endl;

	cout << "sizeof RlcMacDownlink                         " << sizeof(RlcMacDownlink_t) << endl;
	cout << "sizeof Packet_Access_Reject_t                 " << sizeof(Packet_Access_Reject_t) << endl;
       	cout << "sizeof Packet_Cell_Change_Order_t             " << sizeof(Packet_Cell_Change_Order_t) << endl;
       	cout << "sizeof Packet_Downlink_Assignment_t           " << sizeof(Packet_Downlink_Assignment_t) << endl;
	cout << "sizeof Packet_Measurement_Order_Reduced_t     " << sizeof(Packet_Measurement_Order_Reduced_t) << endl;
	cout << "sizeof Packet_Neighbour_Cell_Data_t           " << sizeof(Packet_Neighbour_Cell_Data_t) << endl;
	cout << "sizeof Packet_Serving_Cell_Data_t             " << sizeof(Packet_Serving_Cell_Data_t) << endl;
	cout << "sizeof Packet_Paging_Request_t                " << sizeof(Packet_Paging_Request_t) << endl;
	cout << "sizeof Packet_PDCH_Release_t                  " << sizeof(Packet_PDCH_Release_t) << endl;
	cout << "sizeof Packet_Polling_Request_t               " << sizeof(Packet_Polling_Request_t) << endl;
	cout << "sizeof Packet_Power_Control_Timing_Advance_t  " << sizeof(Packet_Power_Control_Timing_Advance_t) << endl;
	cout << "sizeof Packet_PRACH_Parameters_t              " << sizeof(Packet_PRACH_Parameters_t) << endl;
	cout << "sizeof Packet_Queueing_Notification_t         " << sizeof(Packet_Queueing_Notification_t) << endl;
	cout << "sizeof Packet_Timeslot_Reconfigure_t          " << sizeof(Packet_Timeslot_Reconfigure_t) << endl;
	cout << "sizeof Packet_TBF_Release_t                   " << sizeof(Packet_TBF_Release_t) << endl;
	cout << "sizeof Packet_Uplink_Ack_Nack_t               " << sizeof(Packet_Uplink_Ack_Nack_t) << endl;
	cout << "sizeof Packet_Uplink_Assignment_t             " << sizeof(Packet_Uplink_Assignment_t) << endl;
	cout << "sizeof Packet_Cell_Change_Continue_t          " << sizeof(Packet_Cell_Change_Continue_t) << endl;
	cout << "sizeof Packet_Handover_Command_t              " << sizeof(Packet_Handover_Command_t) << endl;
	cout << "sizeof Packet_PhysicalInformation_t           " << sizeof(Packet_PhysicalInformation_t) << endl;
	cout << "sizeof Packet_Downlink_Dummy_Control_Block_t  " << sizeof(Packet_Downlink_Dummy_Control_Block_t) << endl;
	cout << "sizeof PSI1_t                " << sizeof(PSI1_t) << endl;
	cout << "sizeof PSI2_t                " << sizeof(PSI2_t) << endl;
	cout << "sizeof PSI3_t                " << sizeof(PSI3_t) << endl;
	cout << "sizeof PSI3_BIS_t            " << sizeof(PSI3_BIS_t) << endl;
	cout << "sizeof PSI4_t                " << sizeof(PSI4_t) << endl;
	cout << "sizeof PSI13_t               " << sizeof(PSI13_t) << endl;
	cout << "sizeof PSI5_t                " << sizeof(PSI5_t) << endl;
	*/


	BitVector vectorU[10];
	BitVector vectorD[10];
        BitVector resultVector(23*8);


	//DOWNLINK

        // Packet Downlink Assignment
	vectorD[0].resize(23*8);
	vectorD[0].unhex("4e082500e3f1a81d080820800b2b2b2b2b2b2b2b2b2b2b");
        // Packet Uplink Assignment
	vectorD[1].resize(23*8);
	vectorD[1].unhex("48282407a6a074227201000b2b2b2b2b2b2b2b2b2b2b2b");
	// Packet Uplink Ack Nack
	vectorD[2].resize(23*8);
	vectorD[2].unhex("47240c00400000000000000079eb2ac9402b2b2b2b2b2b");
        // Packet Uplink Assignment
	vectorD[3].resize(23*8);
	vectorD[3].unhex("47283c367513ba333004242b2b2b2b2b2b2b2b2b2b2b2b");

	cout << " DOWNLINK " << endl;
	for (int i = 0; i < 4; i++)
	{
	        RlcMacDownlink_t * data = (RlcMacDownlink_t *)malloc(sizeof(RlcMacDownlink_t));
		cout << "=========Start DECODE===========" << endl;
		decode_gsm_rlcmac_downlink(&vectorD[i], data);
		cout << "+++++++++Finish DECODE++++++++++" << endl;
		cout << "=========Start ENCODE=============" << endl;
	        encode_gsm_rlcmac_downlink(&resultVector, data);
		cout << "+++++++++Finish ENCODE+++++++++++" << endl;
		cout << "vector1 = " <<  vectorD[i] << endl;
	        cout << "vector2 = " <<  resultVector << endl;
		resultVector.unhex("2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b");
		delete data;
	}


	// UPLINK

        // Packet Uplink Dummy Control Block message
	vectorU[0].resize(23*8);
	vectorU[0].unhex("400e1e61d11f2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b");

	// Packet Downlink Ack/Nack message
	vectorU[1].resize(23*8);
	vectorU[1].unhex("400b8020000000000000002480e00b2b2b2b2b2b2b2b2b");
	
	// Packet Resource Request
	vectorU[2].resize(23*8);
        vectorU[2].unhex("4016713dc094270ca2ae57ef909006aa0fc0001f80222b");
	resultVector.unhex("2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b");

	cout << " UPLINK " << endl;
	for (int i = 0; i < 3; i++)
	{
	        RlcMacUplink_t * data = (RlcMacUplink_t *)malloc(sizeof(RlcMacUplink_t));
		cout << "=========Start DECODE===========" << endl;
		decode_gsm_rlcmac_uplink(&vectorU[i], data);
		cout << "+++++++++Finish DECODE++++++++++" << endl;
		cout << "=========Start ENCODE=============" << endl;
	        encode_gsm_rlcmac_uplink(&resultVector, data);
		cout << "+++++++++Finish MAKE+++++++++++" << endl;
		cout << "vector1 = " <<  vectorU[i] << endl;
	        cout << "vector2 = " <<  resultVector << endl;
		resultVector.unhex("2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b");
		delete data;
	}

}
