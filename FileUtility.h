#pragma once
#include <fstream>
#include <vector>

class FileUtility
{
public:
	static std::vector<char> readFile(const std::string& fileName);
};

