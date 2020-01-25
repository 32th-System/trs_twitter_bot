#include <conio.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define NOGDI

	#ifndef UNICODE
	#define UNICODE
	#endif

	#ifndef _UNICODE
	#define _UNICODE
	#endif

	#include <synchapi.h>
	#define SLEEP(t) Sleep(t * 1000)
#else
	#include <unistd.h>
	#define SLEEP sleep
#endif // _WIN32


size_t __cdecl request_return_callback(char *data, size_t size, size_t nmemb, char *out) {
	static size_t buffer_size = CURL_MAX_WRITE_SIZE;
	while (strlen(out) + strlen(data) >= buffer_size) {
		realloc(out, buffer_size + CURL_MAX_WRITE_SIZE);
		buffer_size += CURL_MAX_WRITE_SIZE;
	}
	strncat(out, data, nmemb);
	return nmemb;
}

int main(void) {
	/*
	 *	String literals that one would want to change
	 */
	const char *twitch_api_url = "https://api.twitch.tv/helix/streams?user_id=134711915";
	const char *tweet_args = "%s -d \"status=TRS is now streaming: \\\"%s\\\" https://twitch.tv/Touhou_Replay_Showcase\" /1.1/statuses/update.json";

	
	/*  Obtaining the location of twurl
	 *
	 *	I could have some epic system for determing the proper path to it
	 *	Or I could just tell people to set an environemnt variable themselves
	 */

	char *twurl_loc = getenv("TWURL_LOC");
	if (!twurl_loc) {
		puts("Environment variable TWURL_LOC is not set! Set it to use this bot");
		return 1;
	}

	// libcurl configuration here so I don't have to free anything if this fails
	curl_global_init(CURL_GLOBAL_DEFAULT);
	CURL *curl = curl_easy_init();
	if (!curl) {
		puts("curl_easy_init failed!");
		return 1;
	}


	// Twitch API request header

	// Part 1: Loading the client ID from a file
	FILE *twitch_client_id_file = fopen("twitch_client_id.txt", "rb");
	if (!twitch_client_id_file) {
		puts("twitch_client_id.txt is not present in the current directory or failed to open!");
		return 1;
	}
	fseek(twitch_client_id_file, 0, SEEK_END);
	size_t twitch_client_id_size = ftell(twitch_client_id_file);
	fseek(twitch_client_id_file, 0, SEEK_SET);
	char *twitch_client_id = (char*)_malloca(twitch_client_id_size + 1);
	twitch_client_id[twitch_client_id_size] = 0;
	fread(twitch_client_id, 1, twitch_client_id_size, twitch_client_id_file);
	fclose(twitch_client_id_file);

	// Part 2: sprintf
	char header[46];
	snprintf(header, 46, "Client-ID: %s", twitch_client_id);
	
	// Part 3: free
	_freea(twitch_client_id);

	
	// libcurl config

	// Part 1: Response header and request URL
	struct curl_slist *chunk = NULL;
	chunk = curl_slist_append(chunk, header);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_URL, twitch_api_url);

	// Part 2: Collecting the returned data in a non janky way


	char *ret_data = (char*)malloc(CURL_MAX_WRITE_SIZE );
	ret_data[0] = '\x00';

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_return_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ret_data);

	// Without this variable the bot would try to tweet every 30 seconds when the stream is running
	register char stream_running = 0;


	register size_t tweet_cmd_len = strlen(twurl_loc) + strlen(tweet_args);
	char *tweet_cmd = (char*)_malloca(tweet_cmd_len);
	snprintf(tweet_cmd, tweet_cmd_len, tweet_args, twurl_loc, "%s");

	size_t tweet_cmd_buf_size = strlen(tweet_cmd) + 280;
	char *tweet_cmd_buf = (char*)_malloca(tweet_cmd_buf_size);

	puts("Hold any key for 30 seconds or less to quit gracefully");

	while(!_kbhit()) {
		SLEEP(30);
		if(remove("trs_bot_quit.tmp") = 0) {
			break;
		}
		curl_easy_perform(curl);
		
		/*
		 *	If the Twitch account is not streaming,
		 *	the API will return "{"data":[], "pagination": {}}"
		 *
		 *	If the account if streaming, the API will return JSON that
		 *	contains: "title":"Streamtitle".
		 */

		char *title = strstr(ret_data, "title");
		if (!stream_running && title) {
			printf("Stream started. Full Twitch API response: \n%s\n\n", ret_data);
			title += 8;
			int i;
			for (i = 0; (title[i] != '"') || (title[i - 1] == '\\'); i++);
			title[i] = '\x00';
			snprintf(tweet_cmd_buf, tweet_cmd_buf_size, tweet_cmd, title);
			puts("Running twurl. Twitter response:\n\n\n");
			system(tweet_cmd_buf);
			puts("\n\n\n\n\n");
			stream_running++;
		} else if(!title && stream_running) {
			stream_running = 0;
		}
		ret_data[0] = '\x00';
	}
	
	// Cleanup
	free(ret_data);
	curl_easy_cleanup(curl);
	curl_slist_free_all(chunk);
	return 0;
}
