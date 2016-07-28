#pragma once
#include <memory>
#include <string>

struct IHttp {
	virtual std::string get(std::string url) = 0;
	virtual std::string getToFile(std::string url, FILE* file, std::function<void(unsigned int)> fileposfunc) = 0;
};

std::unique_ptr<IHttp> createHttp();
