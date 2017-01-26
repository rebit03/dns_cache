#pragma once

#include <string>

namespace Cache {

	class ICache {
	public:
		virtual void update(const std::string& name, const std::string& data) = 0;
		virtual std::string resolve(const std::string& name) = 0;
	};

}
