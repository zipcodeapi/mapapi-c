/** Copyright Redline13, LLC */

#include "redline13_mapapi.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/easy.h>

// API endpoint
#define REDLINE13_MAPAPI_ENDPOINT_MAX_LEN 1024
static char map_endpoint[REDLINE13_MAPAPI_ENDPOINT_MAX_LEN] = "https://realtimemapapi.redline13.com:4434";

// Map id and key lengths
#define REDLINE13_MAPAPI_MAP_ID_LEN 8
#define REDLINE13_MAPAPI_MAP_KEY_LEN 16

// Map id and key
static char map_id[REDLINE13_MAPAPI_MAP_ID_LEN+1] = "";
static char map_key[REDLINE13_MAPAPI_MAP_KEY_LEN+1] = "";

/** Initialize map API */
void redline13_mapapi_init(const char * your_map_id, const char * your_map_key)
{
	strncpy(map_id, your_map_id, REDLINE13_MAPAPI_MAP_ID_LEN);
	map_id[REDLINE13_MAPAPI_MAP_ID_LEN] = '\0';
	strncpy(map_key, your_map_key, REDLINE13_MAPAPI_MAP_KEY_LEN);
	map_key[REDLINE13_MAPAPI_MAP_KEY_LEN] = '\0';
	
	// Set up curl
	curl_global_init(CURL_GLOBAL_ALL);
}

/** Set map API endpoint */
void redline13_mapapi_set_endpoint(const char * your_api_endpoint)
{
	strncpy(map_endpoint, your_api_endpoint, REDLINE13_MAPAPI_ENDPOINT_MAX_LEN);
	map_endpoint[REDLINE13_MAPAPI_ENDPOINT_MAX_LEN-1] = '\0';
}

/* Send redline map data  */
struct curl_resp_string { char * str; size_t len; };

/** Curl write callback */
static size_t curl_write_callback_func(void *buffer, size_t size, size_t nmemb, void *resp_arg)
{
	// Make reference to the "resp" variable in redline13_mapapi_send_raw_map_data
	struct curl_resp_string * resp = (struct curl_resp_string *)resp_arg;
	
	// Start index
	size_t start_idx = 0;
	
	// Allocate memory
	if (resp->str == NULL)
	{
		resp->len = size*nmemb;
		resp->str = (char*)malloc((resp->len+1)*sizeof(char));
	}
	else
	{
		// Set start index
		start_idx = resp->len - 1;
		
		// Resize string
		resp->len += size*nmemb;
		resp->str = (char*)realloc(resp->str, (resp->len+1)*sizeof(char));
	}
	
	// Copy data
	memcpy(resp->str + start_idx*sizeof(char), buffer, size*nmemb);
	resp->str[resp->len] = '\0';
	
	return size*nmemb;
}

static const char * point_format = "{}";

/** Convert point to JSON */
static char * convert_point_to_json(redline13_mapapi_point point)
{
	// Calculate min length on pass 1
	int min_len = 2;	// Account for '[', ']', and '\0'.  But we replace the last comma, so less 1
	int pos = 0;
	char * str = NULL;
	
	// Do 2 passes
	short i = 0;
	for (; i < 2; i++)
	{
		// Allocate string
		if (i == 1)
		{
			str = (char*)malloc(min_len*sizeof(char));
			str[0] = '{';
			pos = 1;
		}
		
		///// Location type /////
		// Lat/lng
		if (point.location_type == REDLINE13_POINT_LOCATION_TYPE_COORD)
		{
			if (i == 0)
				min_len += 34;
			else
			{
				pos += sprintf(str+pos, "\"lat\":%0.06f,\"lng\":%0.06f,", point.location.coord.lat, point.location.coord.lng);
			}
		}
		// Zipcode
		else if (point.location_type == REDLINE13_POINT_LOCATION_TYPE_ZIPCODE)
		{
			if (i == 0)
				min_len += 18;
			else
			{
				sprintf(str+pos, "\"zipcode\":\"%05d\",", point.location.zipcode);
				pos += 18;
			}
		}
		
		// Radius
		if (point.r != 0)
		{
			int tmp = point.r / 10;
			int digits = 1;
			while (tmp > 0)
			{
				digits++;
				tmp /= 10;
			}
			if (i == 0)
				min_len += 5 + digits;
			else
			{
				sprintf(str+pos, "\"r\":%d,", point.r);
				pos += 5 + digits;
			}
		}
		
		// Color
		if (point.c != NULL)
		{
			if (i == 0)
				min_len += 14;
			else
			{
				sprintf(str+pos, "\"c\":\"#%06x\",", *point.c);
				pos += 14;
			}
		}
		
		// Color 2
		if (point.c2 != NULL)
		{
			if (i == 0)
				min_len += 15;
			else
			{
				sprintf(str+pos, "\"c2\":\"#%06x\",", *point.c2);
				pos += 15;
			}
		}
		
		// Delay
		if (point.delay != 0)
		{
			int tmp = point.delay / 10;
			int digits = 1;
			while (tmp > 0)
			{
				digits++;
				tmp /= 10;
			}
			if (i == 0)
				min_len += 9 + digits;
			else
			{
				sprintf(str+pos, "\"delay\":%d,", point.delay);
				pos += 9 + digits;
			}
		}
		
		// visible_time
		if (point.visible_time != 0)
		{
			int tmp = point.visible_time / 10;
			int digits = 1;
			while (tmp > 0)
			{
				digits++;
				tmp /= 10;
			}
			if (i == 0)
				min_len += 16 + digits;
			else
			{
				sprintf(str+pos, "\"visible_time\":%d,", point.visible_time);
				pos += 16 + digits;
			}
		}
		
		// Close if off
		if (i == 1)
		{
			str[pos-1] = '}';
			str[pos] = '\0';
		}
	}
	
	return str;
}

/** Send points.  Returns zero on success. */
int redline13_mapapi_send_map_points(redline13_mapapi_point * points, int num_points)
{
	int rtn = 0;
	
	// Generate point strings
	char ** points_strs = (char **)malloc(num_points*sizeof(char *));
	int i = 0;
	int len = num_points + 1; // '[', num_points - 1 commas, and ']'
	for (; i < num_points; i++)
	{
		points_strs[i] = convert_point_to_json(points[i]);
		len += strlen(points_strs[i]);
	}
	
	// Generate JSON string and free individual strings
	char * points_json_str = (char *)malloc(len);
	points_json_str[0] = '[';
	points_json_str[1] = '\0';
	for (i = 0; i < num_points; i++)
	{
		strcat(points_json_str, points_strs[i]);
		strcat(points_json_str, ",");
		free(points_strs[i]);
	}
	points_json_str[len-1] = ']';
	
	// Free points strings
	free(points_strs);
	
	// Create API post data
	len += 31 + REDLINE13_MAPAPI_MAP_ID_LEN + REDLINE13_MAPAPI_MAP_KEY_LEN;
	char * postdata = (char *)malloc(len * sizeof(char));
	sprintf(postdata, "{\"mapId\":\"%s\",\"key\":\"%s\",\"points\":%s}", map_id, map_key, points_json_str);
	
	// Free points json string
	free(points_json_str);
	
	// Send request
	char * resp = redline13_mapapi_send_raw_map_data(postdata);
	if (resp == NULL)
	{
		// Set return value
		rtn = 1;
	}
	else
	{
		if (strcmp(resp, "true") != 0)
		{
			// Set return value
			rtn = 1;
		}
		free(resp);
	}
	
	// Free post data
	free(postdata);
	
	return rtn;
}

/** Send raw redline map data.  Returns raw response.  Caller should free memory for response. */
char * redline13_mapapi_send_raw_map_data(char * postData)
{
	CURL *curl_handle = NULL;
	struct curl_resp_string resp;
	memset(&resp, 0, sizeof(resp));
	
	curl_handle = curl_easy_init();
	if (curl_handle)
	{
		curl_easy_setopt(curl_handle, CURLOPT_URL, map_endpoint);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		
		// Set up POST
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postData);
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		
		// Set up write function
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_write_callback_func);
		
		// Pass data to callbcak
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &resp);
		
		// Run
		curl_easy_perform(curl_handle);
		
		// Free headers
		curl_slist_free_all(headers);
		
		// Cleanup
		curl_easy_cleanup(curl_handle);
	}
	
	return resp.str;
}
