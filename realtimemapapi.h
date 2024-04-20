/** Copyright FastZip, LLC */

#ifndef REALTIMEMAPAPI_H
#define REALTIMEMAPAPI_H

#ifdef __cplusplus
extern "C"{
#endif

/** Lat/long location type */
#define REALTIMEMAPAPI_POINT_LOCATION_TYPE_COORD 1
#define REALTIMEMAPAPI_POINT_LOCATION_TYPE_ZIPCODE 2

#define REALTIMEMAPAPI_COLOR_RGB(r,g,b) ((r << 16) + (g << 8) + b)
#define REALTIMEMAPAPI_COLOR_HEX_PARSE(s) (strlen(s) == 7 && (s)[0] == '#' ? strtol(s+1, NULL, 16) : 0)

/** RealTimeMapAPI Point */
typedef struct realtimemapapi_mapapi_point {
	short location_type;
	union {
		struct {
			float lat;
			float lng;
		} coord;
		unsigned int zipcode;
	} location;
	int r;
	unsigned int * c;
	unsigned int * c2;
	unsigned int delay;
	unsigned int visible_time;
} realtimemapapi_mapapi_point;

/** Initialize map API */
void realtimemapapi_mapapi_init(const char * your_map_id, const char * your_map_key);

/** Set map API endpoint */
void realtimemapapi_mapapi_set_endpoint(const char * your_api_endpoint);

/** Send points.  Returns zero on success. */
int realtimemapapi_mapapi_send_map_points(realtimemapapi_mapapi_point * points, int num_points);

/** Send raw API map data.  Returns raw response.  Caller should free memory for response. */
char * realtimemapapi_mapapi_send_raw_map_data(char * postData);

#ifdef __cplusplus
}
#endif

#endif
