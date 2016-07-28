#include "http.h"

#include <curl/curl.h>

static bool run_curl_global_init = true;



class Curl : public IHttp {
	CURL *_curl;
	CURLcode res;
	std::string _pagedata;

	static size_t get_data(void* buffer, size_t size, size_t nmemb, void* userp) {
		auto ptr = static_cast<Curl*>(userp);
		ptr->_pagedata.append((const char*)buffer, size*nmemb);
		return size*nmemb;
	}

public:
	Curl() {
		if (run_curl_global_init) {
			curl_global_init(CURL_GLOBAL_ALL);
			run_curl_global_init = false;
		}
		_curl = curl_easy_init();

		// General settings
		curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, false);
	}

	~Curl() {
		curl_easy_cleanup(_curl);
	}


	std::string get(std::string url) override {
		_pagedata.clear();
		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, get_data);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
		res = curl_easy_perform(_curl);
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		return _pagedata;
	}

	struct xferinfodata {
		FILE* targetfile;
		std::function<void(int)> fileposfunc;
	};

	/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
	static int xferinfo(void *p, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
	{
		static timeval last_tv = {0, 0};
		timeval tv;
		timeval tv_diff;
		auto data = (xferinfodata*)p;

		gettimeofday(&tv, NULL);
		timersub(&tv, &last_tv, &tv_diff);

		unsigned int diff_ms = tv_diff.tv_sec*1000 + tv_diff.tv_usec/1000;
		if (diff_ms >= 300) {
			auto kb = ftell(data->targetfile)/1024;
			data->fileposfunc(kb);
			last_tv = tv;
		}

		return 0;
	}

	virtual std::string getToFile(std::string url, FILE* file, std::function<void(unsigned int)> fileposfunc) override {
		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, nullptr);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
		struct xferinfodata data;
		data.targetfile = file;
		data.fileposfunc = fileposfunc;
		curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(_curl, CURLOPT_XFERINFODATA, &data);
		curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 0L);
		res = curl_easy_perform(_curl);
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, nullptr);
		curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1L);
		return _pagedata;
	}

};

std::unique_ptr<IHttp> createHttp() {
	return std::unique_ptr<IHttp>(new Curl);
}
