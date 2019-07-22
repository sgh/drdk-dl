#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <map>
#include <sstream>
#include <sys/time.h>
#include <assert.h>

#include "drdk-dl.h"

#include "http.h"
#include "json.h"
#include <openssl/sha.h>
#include <openssl/aes.h>


#ifdef __WIN32
	#include <QCoreApplication>
#endif
using namespace std;

static bool _debug = false;


static unsigned char hex2int(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static void hex2bin(unsigned char* dst, const char* src, unsigned int len) {
	assert((len & 1) == 0);
	while (len>0) {
		*dst = hex2int(src[0])*16 + hex2int(src[1]);
		dst++;
		src += 2;
		len -= 2;
	}
}

std::string decrypt_uri(const std::string& encrypted_uri) {
	unsigned int n = 0;
	for (auto idx=0; idx<8; idx++) {
		n <<= 4;
		n += hex2int(encrypted_uri[idx + 2]);
	}

// 	printf("n: %d\n", n);

	auto a = &encrypted_uri[10 + n];
// 	printf("a: %s\n", a);


	char key_tmp[128];
	sprintf(key_tmp, "%s:sRBzYNXBzkKgnjj8pGtkACch", a);

	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX shactx;
	SHA256_Init(&shactx);
	SHA256_Update(&shactx, key_tmp, strlen(key_tmp));
	SHA256_Final(digest, &shactx);

	unsigned char iv[128];
	hex2bin(iv, a, strlen(a));

	auto cipertext = (unsigned char*)malloc(encrypted_uri.size());
	hex2bin(cipertext, &encrypted_uri[10], n);

	std::string plaintext;
	plaintext.resize(n/2);

	AES_KEY aes_key;
	AES_set_decrypt_key(digest, 256, &aes_key);
	AES_cbc_encrypt(cipertext, (unsigned char*)&plaintext[0], plaintext.size(), &aes_key, iv, 0);

	free(cipertext);

	auto pos = plaintext.find('?');
	if (pos != std::string::npos)
		plaintext.resize(pos);

	return plaintext;
}


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
		if (_debug)
			printf("Line: %s\n", line.c_str());
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

		if (line.find("EXT-X-MEDIA:TYPE=SUBTITLES") != string::npos) {
			size_t start_idx = line.find("URI=\"");
			if (start_idx == string::npos)
				continue;
			start_idx += 5;
			int end_idx = line.find("\"", start_idx);
			std::string uri = line.substr(start_idx, end_idx-start_idx).c_str();
			const char* subtype_key = "subtitleType=";
			auto subtype_idx = uri.find(subtype_key);
			if (subtype_idx != std::string::npos) {
				auto end_pos = uri.find('&', subtype_idx);
				subtype_idx += strlen(subtype_key);
				auto type = uri.substr(subtype_idx, end_pos);
//				printf("SubtitleType=%s\n", type.c_str());
				meta.subtitle_uri[type] = uri;
				if (_debug)
					printf("Subtitle: [%s] %s\n", type.c_str(), uri.c_str());
			}
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


void trim(std::string& s) {
	while (s.size() > 0 && s[s.size()-1] == '\r')
		s.resize(s.size()-1);
}

void fetch_video(IHttp* http, struct video_meta& meta, const string& playlist, const std::string& extension) {
	string targetfilename = meta.program_name + extension;
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
		trim(line);
		if (line[0] != '#') {
			if (_debug)
				printf("URL: %s\n", line.c_str());
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

	if (_debug)
		printf("Extracting targets: \n%s\n", pagedata.c_str());
	extract_targets(meta, pagedata);



	printf("Getting list of playlists\n");
	// HLS - Apple HTTP Live Streaming.
	// HDS - Adobe HTTP Dynamic Streaming.
	//
	// Only HLS works for now.
	pagedata = http->get(meta.uri["HLS"]);

	extract_playlist(meta, pagedata);
	if (meta.playlists.empty()) {
		fprintf(stderr, "No playlists found\n");
		return EXIT_FAILURE;
	}

	map<unsigned int, string>::reverse_iterator it = meta.playlists.rbegin();
	string playlist_uri = it->second;
	printf("Using bandwidth=%d\n", it->first);

	// Subtitles
	if (!meta.subtitle_uri.empty()) {
		auto uri = meta.subtitle_uri["Foreign"];
		if (uri != "") {
#if 0
			// Oldstyle 1 indirection subtitles
			pagedata = http->get(uri.c_str());
			fetch_video(http.get(), meta, pagedata, ".sub");
#else
			fetch_video(http.get(), meta, uri, ".sub");
#endif
		}
	}

	// Video
	pagedata = http->get(playlist_uri.c_str());
	fetch_video(http.get(), meta, pagedata, ".m4v");


	return 0;
}
