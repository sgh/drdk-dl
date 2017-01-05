#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <map>
#include <sstream>
#include <sys/time.h>

#include "drdk-dl.h"

#include "http.h"
#include "json.h"

#ifdef __WIN32
	#include <QCoreApplication>
#endif
using namespace std;

static bool _debug = false;


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



void fetch_video(IHttp* http, struct video_meta& meta, const string& playlist) {
	string targetfilename = meta.program_name + ".m4v";
	FILE* targetfile;

	for (size_t idx=0; idx<targetfilename.size(); idx++) {
		if (targetfilename[idx] == ':' || targetfilename[idx] == '/')
			targetfilename[idx] = '-';
		if (targetfilename[idx] == '?')
			targetfilename.erase(idx, 1);
	}

	printf("Saving to file: %s\n", targetfilename.c_str());

	targetfile = fopen(targetfilename.c_str(), "w+");

	if (targetfile == NULL) {
		fprintf(stderr, "Error opening file.");
		return;
	}

	string line;
	istringstream stream(playlist);
	while (getline(stream, line)) {
		if (line[0] != '#') {
			http->getToFile(line, targetfile, [](unsigned int kb) { printf("\r%u KB", kb); fflush(stdout); });
		}
	}
	fclose(targetfile);

	printf("\n");

}


int main(int argc, char *argv[])
{
#ifdef __WIN32
	QCoreApplication app(argc, argv);
#endif
	if (argc < 2) {
		fprintf(stderr, "Usage: drtv-dl <url>");
		return EXIT_FAILURE;
	}

	auto http = createHttp();
	auto pagedata = http->get(argv[1]);

	struct video_meta meta;
	if (!extract_html_metadata(meta, pagedata)) {
		fprintf(stderr,"No data resource available\n");
		return EXIT_FAILURE;
	}

 	printf("Getting data from %s\n", meta.resource.c_str());
	pagedata = http->get(meta.resource);
	extract_targets(meta, pagedata);


	printf("Getting list of playlists\n");
	pagedata = http->get(meta.uri["HLS"]);

	extract_playlist(meta, pagedata);
	if (meta.playlists.empty()) {
		fprintf(stderr, "No playlists found\n");
		return EXIT_FAILURE;
	}

	map<unsigned int, string>::reverse_iterator it = meta.playlists.rbegin();
	string playlist_uri = it->second;
	printf("Using bandwidth=%d\n", it->first);

	pagedata = http->get(playlist_uri.c_str());

	fetch_video(http.get(), meta, pagedata);

	return 0;
}
