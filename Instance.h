#pragma once

#include "Common.h"

class Instance
{
public:
	static VkInstance& GetDefaultVkInstance();

private:
	static VkInstance mDefaultInstance;
};
