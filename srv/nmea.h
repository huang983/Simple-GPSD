#ifndef NMEA_H
#define NMEA_H

/* Message Classes */
#define NMEA_CLASS_STD         0xF0
#define NMEA_CLASS_PBX         0xF1

/* Message IDs */
#define NMEA_ID_GGA             0x00
#define NMEA_ID_GLL             0x01
#define NMEA_ID_GSA             0x02
#define NMEA_ID_GSV             0x03
#define NMEA_ID_RMC             0x04

#define NMEA_TID_OFFSET         1
#define NMEA_MSG_OFFSET         3
#define NMEA_DELIMETER          ','

/* GSA related */
#define NMEA_GSA_OP_OFFSET      7
#define NMEA_GSA_NAV_OFFSET     8

#endif /* NMEA_H */