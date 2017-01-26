#pragma once

#include "cache.h"

#include <string>

namespace Cache {
	namespace DNS {

		extern const size_t DNS_CACHE_SIZE;

		CCache& getDNSCache() {
			static CCache dnsCache(DNS_CACHE_SIZE);
			return dnsCache;
		}

	}
}
