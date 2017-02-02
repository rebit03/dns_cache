#pragma once

#include "cache_intf.h"

#include <string>
#include <vector>
#include <set>
#include <memory>
#include <shared_mutex>

namespace Cache {

	class CCache : public ICache {
	public:
		explicit CCache(size_t max_size);
		virtual ~CCache();
		CCache(const CCache&) = delete;
		CCache& operator=(const CCache&) = delete;
	public:
		// name: expected nonempty lowercase string with alphabet letters only
		virtual void update(const std::string& name, const std::string& data) noexcept override;
		virtual std::string resolve(const std::string& name) noexcept override;
	protected:
		struct CEntry;
		using CEntryPtr = std::shared_ptr<CEntry>;
		using ChildrenContainer = std::vector<CEntryPtr>;
	protected:
		CEntryPtr& updateCache(CEntryPtr& entry, const std::string& name, uint32_t position, const std::string& data);
		CEntryPtr& insertChild(const CEntryPtr& entry, const std::string& name, uint32_t position, const std::string& data);
		CEntryPtr& splitEntry(CEntryPtr& entry, const std::string& name, uint32_t position, const std::string& data, uint32_t prefixLen = 0);

		CEntryPtr resolve(const CEntryPtr& entry, const std::string& name, uint32_t position) const;

		void updateLinkedListOnAccess(const CEntryPtr& entry) noexcept;
		void updateLinkedListOnRemove(const CEntryPtr& entry) noexcept;

		void removeEntry(CEntryPtr entry);
		void removeEntry(const std::string& name);
		// merge the only one child in entry, entry has index index, entry is updated to the merged value
		void mergeChild(CEntryPtr& entry, uint32_t index);

		void getName(const CEntryPtr& entry, std::string& name) const; // gets the name from reverse traversing through the tree
		uint32_t getChildIndex(const CEntryPtr& entry) const noexcept;
		uint32_t getFirstChildIndex(const ChildrenContainer& children) const noexcept;

		void dump() const;
		void dumpCache(const CEntryPtr& entry, std::string name, uint32_t level = 1) const;
		void dumpLinkedList() const;
	protected:
		std::shared_mutex	m_cacheMutex;	// access to the cache
		std::mutex			m_llMutex;		// acess only to linked list
		const size_t		m_maxSize{ 0 };
		size_t				m_currentSize{ 0 };
		CEntryPtr			m_root{ std::make_shared<CEntry>() };
		CEntryPtr			m_head;
		CEntryPtr			m_tail;
	protected:
		struct CEntry {
		public:
			explicit CEntry() {}
			virtual ~CEntry() {}

			inline bool hasParent() const noexcept { return m_parent.operator bool(); }
			inline bool hasLeftSibling() const noexcept { return m_lSibling.operator bool(); }
			inline bool hasRightSibling() const noexcept { return m_rSibling.operator bool(); }
			inline bool hasProxyValue() const noexcept { return !m_proxyValue.empty(); }
			inline bool hasData() const noexcept { return !m_data.empty(); }
			inline bool hasChildren() const noexcept { return m_childrenCount != 0; }
			inline bool isEmpty() const noexcept { return !(hasChildren() || hasProxyValue() || hasData()); };

			inline void clearProxyValue() noexcept { m_proxyValue.clear(); }
			inline void setProxyValue(const std::string& str) { m_proxyValue.assign(str); }
			inline void setProxyValue(const std::string& str, uint32_t pos, size_t count = std::string::npos) { m_proxyValue.assign(str, pos, count); }
			inline void setProxyValue(std::string&& str) { m_proxyValue.swap(str); }

			inline void clearData() noexcept { m_data.clear(); }
			inline void setData(const std::string& data) { m_data.assign(data); }
			inline void setData(std::string&& data) noexcept { m_data.swap(data); }
		protected:
			static const uint32_t ALPHABET_SIZE = 38;	// [a-z][0-9][-.]
		public:
			CEntryPtr			m_parent;
			CEntryPtr			m_lSibling;
			CEntryPtr			m_rSibling;
			std::string			m_proxyValue;
			std::string			m_data;
			uint32_t			m_childrenCount{ 0 };
			ChildrenContainer	m_children{ ALPHABET_SIZE };
		};
	};

}

