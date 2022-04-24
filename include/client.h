#ifndef GPSD_CLIENT_H
#define GPSD_CLIENT_H

typedef enum {
    GPS_NOT_READY = 0, /* GPS module not started yet */
    GPS_POS_FIXED, /* GPS module has locked at least 4 satellites and it's in 3D position mode */
    GPS_TIME_FIXED, /* GPS module has obtained valid TOW, week number, and leap seconds */
    GPS_STATE_NUM,
} GpsState;

#endif // GPSD_CLIENT_H