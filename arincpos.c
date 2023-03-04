/*
 *  Copyright (c) 2023 Thierry Leconte
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <libacars/asn1/FANSATCDownlinkMessage.h>
#include <libacars/asn1/FANSATCDownlinkMsgElementId.h>
#include <libacars/asn1/FANSATCDownlinkMsgElementIdSequence.h>
#include <libacars/asn1/FANSLatitudeLongitude.h>
#include <libacars/asn1/FANSPositionCurrent.h>
#include <libacars/asn1/FANSPositionReport.h>
#include <libacars/libacars.h>
#include <libacars/acars.h>
#include <libacars/vstring.h>
#include <libacars/arinc.h>
#include <libacars/adsc.h>
#include <libacars/list.h>
#include <libacars/cpdlc.h>

#include "acars.h"

static double parse_coordinate(long degrees, long *tenthsofminutes)
{
	double result = (double)degrees;
	if (tenthsofminutes != NULL) {
		result += (double)(*tenthsofminutes) / 10.0f / 60.0f;
	}
	return result;
}

static long parse_altitude(FANSAltitude_t altitude)
{
	long result = -1;
	static const double meters2feet = 3.28084;

	switch (altitude.present) {
	case FANSAltitude_PR_altitudeQNH:
		result = altitude.choice.altitudeQNH * 10;
		break;
	case FANSAltitude_PR_altitudeQNHMeters:
		result =
		    (long)round((double)altitude.choice.altitudeQNHMeters *
				meters2feet);
		break;
	case FANSAltitude_PR_altitudeQFE:
		result = altitude.choice.altitudeQFE * 10;
		break;
	case FANSAltitude_PR_altitudeQFEMeters:
		result =
		    (long)round((double)altitude.choice.altitudeQFEMeters *
				meters2feet);
		break;
	case FANSAltitude_PR_altitudeGNSSFeet:
		result = altitude.choice.altitudeGNSSFeet;
		break;
	case FANSAltitude_PR_altitudeGNSSMeters:
		result =
		    (long)round((double)altitude.choice.altitudeGNSSMeters *
				meters2feet);
		break;
	case FANSAltitude_PR_altitudeFlightLevel:
		result = altitude.choice.altitudeFlightLevel * 100;
		break;
	case FANSAltitude_PR_altitudeFlightLevelMetric:
		result =
		    (long)round((double)altitude.choice.
				altitudeFlightLevelMetric * 10.0 * meters2feet);
		break;
	case FANSAltitude_PR_NOTHING:	/* No components present */
	default:
		break;
	}
	return result;
}

void extract_position(FANSPositionReport_t rpt, oooi_t * oooi)
{
	FANSPositionCurrent_t pos = rpt.positioncurrent;
	if (pos.present != FANSPosition_PR_latitudeLongitude) {
		return;
	}
	FANSLatitudeLongitude_t latlon = pos.choice.latitudeLongitude;
	double lat =
	    parse_coordinate(latlon.latitude.latitudeDegrees,
			     latlon.latitude.minutesLatLon);
	if (latlon.latitude.latitudeDirection == FANSLatitudeDirection_south) {
		lat = -lat;
	}
	double lon =
	    parse_coordinate(latlon.longitude.longitudeDegrees,
			     latlon.longitude.minutesLatLon);
	if (latlon.longitude.longitudeDirection == FANSLongitudeDirection_west) {
		lon = -lon;
	}
	oooi->lat = lat;
	oooi->lon = lon;
	oooi->epu = 1;
	int alt = parse_altitude(rpt.altitude);
	if (alt > 0)
		oooi->alt = alt;

}

la_proto_node *arincdecode(char *txt, char *label, char bid, oooi_t * oooi)
{
	la_proto_node *lanode;
	la_msg_dir direction;
	char sublabel[3];
	char mfi[3];

	if (txt[0] == 0)
		return NULL;

	if (bid >= '0' && bid <= '9')
		direction = LA_MSG_DIR_AIR2GND;
	else
		direction = LA_MSG_DIR_GND2AIR;

	int offset = la_acars_extract_sublabel_and_mfi(label, direction, txt,
						       strlen(txt), sublabel,
						       mfi);
	char *ptr = txt;
	if (offset > 0) {
		ptr += offset;
	}
	lanode = la_acars_decode_apps(label, ptr, direction);
	if (lanode == NULL)
		return NULL;

	/* try to get adsc position */
	la_proto_node *adsc_node = la_proto_tree_find_adsc(lanode);
	if (adsc_node) {

		la_adsc_msg_t *msg = (la_adsc_msg_t *) adsc_node->data;
		la_list *l = msg->tag_list;
		la_adsc_basic_report_t *rpt = NULL;
		while (l != NULL) {
			la_adsc_tag_t *tag_struct = (la_adsc_tag_t *) l->data;
			uint8_t t = tag_struct->tag;
			// These tags contain a Basic Report
			if (t == 7 || t == 9 || t == 10 || t == 18 || t == 19
			    || t == 20) {
				rpt =
				    (la_adsc_basic_report_t *) tag_struct->data;
				break;
			}
			l = la_list_next(l);
		}
		if (rpt) {
			oooi->epu = 1;
			oooi->lat = rpt->lat;
			oooi->lon = rpt->lon;
			oooi->alt = rpt->alt;

			return lanode;
		}
	}

	/* try to get cpdlc position */
	la_proto_node *cpdlc_node = la_proto_tree_find_cpdlc(lanode);
	if (cpdlc_node) {

		FANSATCDownlinkMessage_t *dm = NULL;
		la_cpdlc_msg *msg = (la_cpdlc_msg *) cpdlc_node->data;
		if (msg->err != true)
			dm = (FANSATCDownlinkMessage_t *) msg->data;
		if (dm) {
			if (dm->aTCDownlinkmsgelementid.present ==
			    FANSATCDownlinkMsgElementId_PR_dM48PositionReport) {
				extract_position(dm->aTCDownlinkmsgelementid.
						 choice.dM48PositionReport,
						 oooi);
				return lanode;
			}

			FANSATCDownlinkMsgElementIdSequence_t *dmeid_seq =
			    dm->aTCdownlinkmsgelementid_seqOf;
			if (dmeid_seq != NULL) {
				for (int i = 0; i < dmeid_seq->list.count; i++) {
					FANSATCDownlinkMsgElementId_t *dmeid_ptr
					    =
					    (FANSATCDownlinkMsgElementId_t *)
					    dmeid_seq->list.array[i];
					if (dmeid_ptr != NULL
					    && dmeid_ptr->present ==
					    FANSATCDownlinkMsgElementId_PR_dM48PositionReport)
					{
						extract_position(dmeid_ptr->
								 choice.
								 dM48PositionReport,
								 oooi);
						return lanode;
					}
				}
			}
		}
	}

	return lanode;
}
