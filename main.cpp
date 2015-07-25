#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <map>
#include <sstream>
#include <sys/time.h>

#include <json-c/json.h>

#include <curl/curl.h>

using namespace std;

static bool _debug = false;

size_t data_ignore(void */*buffer*/, size_t size, size_t nmemb, void* /*userp*/) {
	return size*nmemb;
}

string pagedata;

size_t get_data(void* buffer, size_t size, size_t nmemb, void* /*userp*/) {
// 	printf("%s\n", buffer);
	pagedata.append((const char*)buffer, size*nmemb);
	return size*nmemb;
}


struct video_meta {
	string resource;
	string image;
	string program_name;
	string broadcast_date;
	string material_identifier;
	string program_serie_slug;
	string episode_slug;
	string urn_id;
	string duration_ms;
	string production_number;
	string popup;
	map<string,string> uri;
	map<unsigned int, string> playlists;
};

string get_value(const char* key, const string& data) {
	int start_index;
	int end_index;

	// find key
	start_index = data.find(key);
	if (start_index == -1)
		goto error_out;

	// First first "
	start_index = data.find("\"", start_index+1);
	if (start_index == -1)
		goto error_out;
	start_index++;

	// Find ending "
	end_index = data.find("\"", start_index);
	if (end_index == -1)
		goto error_out;


	return data.substr(start_index, end_index-start_index).c_str();

error_out:
	if (_debug)
		fprintf(stderr, "Error finding; %s\n", key);
	return "";
}

bool extract_html_metadata(struct video_meta& meta, const string& page) {
	meta.resource = get_value("data-resource", page);
	meta.image = get_value("data-image", page);

	meta.program_name        = get_value("data-programme-name", page);
	meta.broadcast_date      = get_value("data-broadcast-date", page);
	meta.material_identifier = get_value("data-material-identifier", page);
	meta.program_serie_slug  = get_value("data-program-serie-slug", page);
	meta.episode_slug        = get_value("data-episode-slug", page);
	meta.urn_id              = get_value("data-urn-id", page);
	meta.duration_ms         = get_value("data-duration-in-milliseconds", page);
	meta.production_number   = get_value("data-production-number", page);
	meta.popup               = get_value("data-popup", page);

	if (meta.resource.empty())
		return false;

	return true;
}

void extract_json_metadata(struct video_meta& meta, const string& page) {
	json_object *new_obj;
	json_object *links_obj;
	new_obj = json_tokener_parse( page.c_str() );

// 	printf("%s\n", pagedata.c_str());

	if (!json_object_object_get_ex(new_obj, "Links", &links_obj)) {
		fprintf(stderr, "Error getting links\n");
		return;
	}

	int nr_links = json_object_array_length(links_obj);
// 	printf("%d links\n", nr_links);
	for (int idx=0; idx<nr_links; idx++) {
		json_object* value = json_object_array_get_idx(links_obj, idx);

		json_object* target_obj;
		if (!json_object_object_get_ex(value, "Target", &target_obj))
			continue;

		json_object* uri_obj;
		if (json_object_object_get_ex(value, "Uri", &uri_obj)) {
			const char* str_target = json_object_get_string(target_obj);
			const char* str_uri    = json_object_get_string(uri_obj);
			if (_debug) {
				printf("Target: %s\n", str_target);
				printf("Uri: %s\n", str_uri);
			}
			meta.uri[str_target] = str_uri;
		}
	}
}


void extract_playlist(struct video_meta& meta, const string& page) {
	unsigned int bandwidth = 0;
	string line;
	istringstream stream(page);
	while (getline(stream, line)) {
		if (line.find("#EXT-X-STREAM-INF:") != string::npos) {
			size_t idx = line.find("BANDWIDTH");
			if (idx == string::npos)
				continue;
			int start_idx = line.find("=", idx+1)+1;
			int end_idx = line.find(",", idx+1);
			bandwidth = atoi( line.substr(start_idx, end_idx-start_idx).c_str() );
			if (_debug)
				printf("Stream info: %s\n",line.c_str());
			continue;
		}

		if (bandwidth != 0) {
			if (_debug)
				printf("Playlist for bandwidth %d: %s\n", bandwidth, line.c_str());
			meta.playlists[bandwidth] = line;
		}
		bandwidth = 0;
	}
}


/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
static int xferinfo(void *p, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	static timeval last_tv = {0, 0};
	timeval tv;
	timeval tv_diff;
	FILE* targetfile = (FILE*)p;

	gettimeofday(&tv, NULL);
	timersub(&tv, &last_tv, &tv_diff);

	unsigned int diff_ms = tv_diff.tv_sec*1000 + tv_diff.tv_usec/1000;
	if (diff_ms >= 300)
	{
		printf("\r%lu KB", ftell(targetfile)/1024);
		fflush(stdout);
		last_tv = tv;
	}

	return 0;
}



void fetch_video(struct video_meta& meta, const string& playlist) {
	string targetfilename = meta.program_name + ".mp4";
	FILE* targetfile;
	CURL *curl;

	curl = curl_easy_init();

	if(!curl)
		return;

	for (size_t idx=0; idx<targetfilename.size(); idx++) {
		if (targetfilename[idx] == ':')
			targetfilename[idx] = '-';
	}

	printf("Saving to file: %s\n", targetfilename.c_str());

	targetfile = fopen(targetfilename.c_str(), "w+");

	if (targetfile == NULL) {
		fprintf(stderr, "Error opening file.");
		return;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, targetfile);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, targetfile);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	string line;
	istringstream stream(playlist);
	while (getline(stream, line)) {
		if (line[0] != '#') {
			retry:
			curl_easy_setopt(curl, CURLOPT_URL, line.c_str());
			CURLcode res = curl_easy_perform(curl);
			if(res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				fprintf(stderr, "retrying...");
				goto retry;
			}
		}
	}
	fclose(targetfile);

	curl_easy_cleanup(curl);
	printf("\n");

}


int main(int argc, char *argv[])
{
	CURL *curl;
	CURLcode res;


	if (argc < 2) {
		fprintf(stderr, "Usage: drtv-dl <url>");
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if(!curl)
		return 0;

	// General settings
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_data);
	curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

	struct video_meta meta;
	if (!extract_html_metadata(meta, pagedata)) {
		fprintf(stderr,"No data resource available\n");
		return EXIT_FAILURE;
	}

	pagedata = "";
 	printf("Getting data from %s\n", meta.resource.c_str());
	curl_easy_setopt(curl, CURLOPT_URL, meta.resource.c_str());
	res = curl_easy_perform(curl);
	if(res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	extract_json_metadata(meta, pagedata);


	printf("Getting list of playlists\n");
	pagedata = "";
	curl_easy_setopt(curl, CURLOPT_URL, meta.uri["HLS"].c_str());
	res = curl_easy_perform(curl);
	if(res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));


	extract_playlist(meta, pagedata);
	if (meta.playlists.empty()) {
		fprintf(stderr, "No playlists found\n");
		return EXIT_FAILURE;
	}

	map<unsigned int, string>::reverse_iterator it = meta.playlists.rbegin();
	string playlist_uri = it->second;
	printf("Using bandwidth=%d\n", it->first);

	pagedata = "";
	curl_easy_setopt(curl, CURLOPT_URL, playlist_uri.c_str());
	res = curl_easy_perform(curl);
	if(res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

	fetch_video(meta, pagedata);

	return 0;
}
