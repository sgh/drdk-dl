#pragma once
#include <string>
#include <map>

struct video_meta {
	std::string resource;
	std::string image;
	std::string program_name;
	std::string broadcast_date;
	std::string material_identifier;
	std::string program_serie_slug;
	std::string episode_slug;
	std::string urn_id;
	std::string duration_ms;
	std::string production_number;
	std::string popup;
	std::map<std::string,std::string> uri;
	std::map<std::string, std::string> subtitle_uri;
	std::map<unsigned int, std::string> playlists;
};
