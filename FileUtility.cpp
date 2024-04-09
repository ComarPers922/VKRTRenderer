#include "FileUtility.h"
#include <filesystem>

std::vector<char> FileUtility::readFile(const std::string& fileName)
{
	try
	{
		std::filesystem::path path(fileName);
		is_empty(path);
	}
	catch (std::filesystem::filesystem_error)
	{
		return { '\0' };
	}

	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file!");
	}

	size_t fileSize = file.tellg();

	std::vector<char> fileBuffer(fileSize + 1, '\0');

	file.seekg(0);
	file.read(fileBuffer.data(), fileSize);

	file.close();
	return fileBuffer;
}
