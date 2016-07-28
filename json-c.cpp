#include "json.h"

#include "drdk-dl.h"

#include <json-c/json.h>

void extract_targets(struct video_meta& meta, const std::string& page) {
	json_object *new_obj;
	json_object *links_obj;
	new_obj = json_tokener_parse( page.c_str() );

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
			meta.uri[str_target] = str_uri;
		}
	}
}

