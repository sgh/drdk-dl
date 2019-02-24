#include "json.h"

#include "drdk-dl.h"

#include <json-c/json.h>


#include <stdio.h>
#include <string>
// #include <string.h>



static void extract_subtitles(struct video_meta& meta, json_object* subtitles_obj) {
	int nr_subtitles = json_object_array_length(subtitles_obj);
	printf("%d subtitles\n", nr_subtitles);
	for (int idx=0; idx<nr_subtitles; idx++) {
		json_object* value = json_object_array_get_idx(subtitles_obj, idx);

		json_object* type_obj;
		json_object* uri_obj;
		if (!json_object_object_get_ex(value, "Type", &type_obj))
			continue;

		if (!json_object_object_get_ex(value, "Uri", &uri_obj))
			continue;

		auto type_str = json_object_get_string(type_obj);
		auto uri_str = json_object_get_string(uri_obj);

		printf("   %s = \"%s\"\n", type_str, uri_str);
		meta.subtitle_uri[type_str] = uri_str;
	}
}

void extract_targets(struct video_meta& meta, const std::string& page) {
	json_object *new_obj;
	json_object *links_obj;
	new_obj = json_tokener_parse( page.c_str() );

	json_object *subtitles_obj;
	json_object_object_get_ex(new_obj, "SubtitlesList", &subtitles_obj);
	extract_subtitles(meta, subtitles_obj);

	if (!json_object_object_get_ex(new_obj, "Links", &links_obj)) {
		fprintf(stderr, "Error getting links\n");
		return;
	}

	int nr_links = json_object_array_length(links_obj);
//	printf("%d links\n", nr_links);
	for (int idx=0; idx<nr_links; idx++) {
		json_object* value = json_object_array_get_idx(links_obj, idx);

		json_object* target_obj;
		if (!json_object_object_get_ex(value, "Target", &target_obj))
			continue;

		json_object* uri_obj;
		std::string target_uri;
		if (target_uri.empty() && json_object_object_get_ex(value, "Uri", &uri_obj)) {
			auto ptr = json_object_get_string(uri_obj);
		    target_uri = ptr ? ptr : "";
		}

		if (target_uri.empty() && json_object_object_get_ex(value, "EncryptedUri", &uri_obj)) {
			auto ptr = json_object_get_string(uri_obj);
		    target_uri = ptr ? decrypt_uri(ptr) : "";
		}

		const char* str_target = json_object_get_string(target_obj);
		printf("Target: %s\n", str_target);
		printf("  Uri: %s\n", target_uri.c_str());
		if (!target_uri	.empty())
			meta.uri[str_target] =  target_uri;
	}
}

