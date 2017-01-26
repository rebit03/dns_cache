#include "cache.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <exception>
#include <stdexcept>
#include <cassert>
#include <mutex>

namespace Cache {

	using namespace std;

	namespace {

		const size_t NUMBERS_INDEX = 26;
		const size_t SPECIAL_INDEX = 36;

		size_t getIndex(const char& ch) noexcept
		{
			size_t index = ch;
			if (index >= 'a') {
				index -= 'a';
			}
			else if (index >= '0') {
				index -= '0' - NUMBERS_INDEX;
			}
			else {
				index -= '-' - SPECIAL_INDEX;
			}
			return index;
		}

		char getChar(size_t index) noexcept
		{
			if (index < NUMBERS_INDEX) {
				index += 'a';
			}
			else if (index < SPECIAL_INDEX) {
				index += '0' - NUMBERS_INDEX;
			}
			else {
				index += '-' - SPECIAL_INDEX;
			}
			return static_cast<char>(index);
		}

		size_t min(size_t l, size_t r) noexcept
		{
			return l > r ? r : l;
		}

		string commonPrefix(const string& l, const string& r)
		{
			auto len(min(l.size(), r.size()));
			auto diff(move(mismatch(l.begin(), l.begin() + len, r.begin())));
			return string(l.begin(), diff.first);
		}

	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	CCache::CCache(size_t max_size)
		: m_maxSize(max_size)
	{
	}

	CCache::~CCache()
	{
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void CCache::update(const string& name, const string& data) noexcept
	{
		try {
			unique_lock<shared_mutex> lock(m_cacheMutex);

			if (data.empty()) {
				clog << "empty IP, invalidating entry for " << name << endl;
				removeEntry(name);
			}
			else {
				clog << "updating " << name << " to " << data << endl;

				const auto& entry(updateCache(m_root, name, 0, data));

				if (!(entry->hasLeftSibling() || entry->hasRightSibling() || entry == m_head)) {
					m_currentSize++;
				} else
				if (entry == m_head && m_currentSize == 0) {
					m_currentSize = 1;
				}

				updateLinkedListOnAccess(entry);

				if (m_currentSize > m_maxSize) {
					dump();
					clog << "max size reached, removing tail" << endl;
					removeEntry(m_tail);
				}
			}

			dump();
		}
		catch (const exception& e) {
			cerr << "failed to update " << name << " to " << data << ": " << e.what() << endl;
		}
	}

	CCache::CEntryPtr& CCache::updateCache(CEntryPtr& entry, const string& name, size_t position, const string& data)
	{
		try {
			if (entry->hasProxyValue()) {
				auto prefix(move(commonPrefix(name.substr(position), entry->m_proxyValue)));

				if (prefix.empty())
				{	// 1. there is already some partial string, but no common prefix with current name -> split entry
					return splitEntry(entry, name, position, data, prefix);
				}

				auto nameLen((name.size() - position)); // lenght of the name from current position

				if (prefix.size() == entry->m_proxyValue.size())
				{	// current partial string matches the common name prefix
					if (prefix.size() == nameLen)
					{	// 2. matched the whole name -> actualize data
						entry->setData(data);
						return entry;
					}
					else // prefix.size() < nameLen
					{	// 3. -> shift position by common prefix and insert the rest of the name
						position += prefix.size();
						return insertChild(entry, name, position, data);
					}
				}
				else // prefix.size() < entry->m_proxyValue.size()
				{	// 4. common prefix with current proxy value are not matching -> shift position by common prefix and split entry
					position += prefix.size();
					return splitEntry(entry, name, position, data, prefix);
				}
			}
			else if (!(entry->hasData() || entry->hasChildren())) {
				// 5. empty entry -> set data
				try {
					entry->setProxyValue(name.substr(position));
					entry->setData(data);
					return entry;
				}
				catch (const exception&) {
					// revert state
					entry->m_proxyValue.clear();
					throw;
				}
			}
			else {
				// 6. no proxy value, no data, but already some children -> insert
				return insertChild(entry, name, position, data);
			}
		}
		catch (const exception&) {
			//cerr << "failed to update " << name << " to " << data << ": " << e.what() << endl;
			throw;
		}
	}

	CCache::CEntryPtr& CCache::insertChild(const CEntryPtr& entry, const string& name, size_t position, const string& data)
	{
		auto index(getIndex(name.at(position)));
		auto& child(entry->m_children[index]);
		position++;

		try {
			if (!child) {
				child = make_shared<CEntry>();
				child->m_parent = entry;
				entry->m_childrenCount++;
			}

			auto nameLen((name.size() - position)); // lenght of the name from current position
			if (nameLen == 0 && !child->hasProxyValue())
			{	// last letter was indexed already and no partial string here, it's a match -> actualize data
				child->setData(data);
				return child;
			}
			else
			{
				return updateCache(child, name, position, data);
			}
		}
		catch (const exception&) {
			if (child && child->isEmpty())
			{	// revert state
				child.reset();
				entry->m_childrenCount--;
			}
			// TODO: throw custom exception
			throw;
		}
	}

	CCache::CEntryPtr& CCache::splitEntry(CEntryPtr& entry, const string& name, size_t position, const string& data, const string& prefix)
	{
		try {
			// create new entry with common prefix
			auto newEntry(make_shared<CEntry>());
			auto nameLen((name.size() - position)); // lenght of the name from current position
			if (nameLen == 0)
			{	// last letter already indexed, it's a match -> set data
				newEntry->setData(data);
			}
			newEntry->setProxyValue(prefix);

			auto originalProxyValue(move(entry->m_proxyValue));
			// set rest of the proxy value after common prefix (minus index letter) to original entry
			entry->setProxyValue(move(originalProxyValue.substr(prefix.size() + 1)));
			// swap parents and set new entry as parent to previous
			newEntry->m_parent.swap(entry->m_parent);
			entry->m_parent = newEntry;
			// swap positions, new entry has to be in original entry position
			entry.swap(newEntry);
			// insert original entry to new entry to correct position in children array, which is now empty
			auto index(getIndex(originalProxyValue.at(prefix.size())));
			entry->m_children[index].swap(newEntry);
			entry->m_childrenCount++;

			if (nameLen == 0)
			{	// nothing to index anymore -> return the newly created entry as it contains inserted data
				return entry;
			}
			else
			{	// now the rest after common prefix has to be indexed in correct child entry of the new entry children array
				try {	// TODO: create custom exception so it is possible to catch it in outer try-catch block
					return insertChild(entry, name, position, data);
				}
				catch (const exception&) {
					// revert state, TODO: check if behaves correctly
					// put splitted entry to its original position
					entry.swap(newEntry);
					entry.swap(newEntry->m_children[index]);
					// set its original parent
					entry->m_parent.swap(newEntry->m_parent);
					// set its original proxy value
					entry->m_proxyValue.swap(originalProxyValue);
					//cerr << "failed to update " << name << " to " << data << ": " << e.what() << endl;
					throw;
				}
			}
		}
		catch (const exception&) {
			//cerr << "failed to create new entry: " << e.what() << endl;
			// TODO: throw custom exception
			throw;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	string CCache::resolve(const string& name) noexcept
	{
		try {
			shared_lock<shared_mutex> lock(m_cacheMutex);

			auto entry(move(resolve(m_root, name, 0)));
			if (entry->hasData()) {
				lock_guard<mutex> lock(m_llMutex);
				updateLinkedListOnAccess(entry);
				dumpLinkedList();
			}
			return entry->m_data;
		}
		catch (const exception& e) {
			cerr << "failed to resolve " << name << ": " << e.what() << endl;
			return string();
		}
	}

	CCache::CEntryPtr CCache::resolve(const CEntryPtr& entry, const string& name, size_t position) const
	{
		auto nameLen((name.size() - position)); // lenght of the name from current position

		if (nameLen == 0 && !entry->hasProxyValue()) {
			return entry;
		}

		if (entry->hasProxyValue()) {
			auto prefix(move(commonPrefix(name.substr(position), entry->m_proxyValue)));
			if (prefix.size() == entry->m_proxyValue.size())
			{
				if (prefix.size() == nameLen)
				{	// matched the whole name
					return entry;
				}
				else // prefix.size() < nameLen
				{	// shift position by common prefix
					position += prefix.size();
					nameLen -= prefix.size();
				}
			}
			else
			{	//  name not found in the tree
				return make_shared<CEntry>();
			}
		}

		auto index(getIndex(name.at(position)));
		const auto& child(entry->m_children[index]);
		if (!child)
		{	//  name not found in the tree
			return make_shared<CEntry>();
		}
		//  move by indexed letter
		position++;
		return resolve(child, name, position);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void CCache::updateLinkedListOnAccess(const CEntryPtr& entry) noexcept
	{
		if (m_head) {
			if (m_head != entry) {
				// moving tail to front -> update tail reference
				if (entry == m_tail) {
					m_tail = m_tail->m_lSibling;
				}
				// unlink entry from the list
				if (entry->hasLeftSibling()) {
					entry->m_lSibling->m_rSibling = entry->m_rSibling;
				}
				if (entry->hasRightSibling()) {
					entry->m_rSibling->m_lSibling = entry->m_lSibling;
				}
				// move entry to the front of the list
				entry->m_rSibling = m_head;
				m_head->m_lSibling = entry;
				m_head = entry;
				// remove circular references
				m_head->m_lSibling.reset();
				m_tail->m_rSibling.reset();
			}
		}
		else {
			m_head = entry;
			m_tail = entry;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void CCache::removeEntry(const string& name)
	{
		auto entry(move(resolve(m_root, name, 0)));
		if (entry->hasData())
		{	// the name is in the tree
			removeEntry(move(entry));
		}
	}

	void CCache::removeEntry(CEntryPtr entry)
	{
		entry->clearData();
		if (!entry->hasChildren()) {
			entry->clearProxyValue();
		}
		m_currentSize--;

		updateLinkedListOnRemove(entry);

		// merge proxy values
		size_t index(0);
		if (entry->hasParent()) {
			index = getChildIndex(entry);

			// remove entry if it's useless - IP is cleared, if there are no children, it's useless
			if (!entry->hasChildren()) {
				entry->m_parent->m_childrenCount--;
				entry->m_parent->m_children[index].reset();
				entry = entry->m_parent;

				if (entry->hasParent()) {
					index = getChildIndex(entry);
				}
			}
		}

		mergeChild(entry, index);
	}

	void CCache::updateLinkedListOnRemove(const CEntryPtr& entry) noexcept
	{
		if (entry->hasLeftSibling()) {
			entry->m_lSibling->m_rSibling = entry->m_rSibling;
			if (m_tail == entry) {
				m_tail = m_tail->m_lSibling;
			}
		}

		if (entry->hasRightSibling()) {
			entry->m_rSibling->m_lSibling = entry->m_lSibling;
			if (m_head == entry) {
				m_head = m_head->m_rSibling;
			}
		}

		entry->m_lSibling.reset();
		entry->m_rSibling.reset();
	}

	size_t CCache::getChildIndex(const CEntryPtr& entry) const noexcept
	{
		assert(entry->hasParent() && "ENTRY HAS TO HAVE PARENT");

		size_t index(0);
		for (const auto& child : entry->m_parent->m_children) {
			if (child == entry) {
				break;
			}
			index++;
		}
		return index;
	}

	void CCache::mergeChild(CEntryPtr& entry, size_t index)
	{
		if (entry->m_childrenCount != 1 || entry->hasData()) {
			return;
		}

		size_t chIndex(getFirstChildIndex(entry->m_children));

		{
			// child proxy = current entry proxy + index letter + child proxy
			string proxy(move(entry->m_proxyValue));
			proxy += getChar(chIndex);
			proxy += entry->m_children[chIndex]->m_proxyValue;

			entry->m_children[chIndex]->setProxyValue(move(proxy));
		}

		entry->m_children[chIndex]->m_parent = entry->m_parent;
		if (entry->hasParent()) {
			entry->m_parent->m_children[index].swap(entry->m_children[chIndex]);
			entry = entry->m_parent->m_children[index];
		}
		else {
			m_root.swap(entry->m_children[chIndex]);
			entry = m_root;
		}
	}

	size_t CCache::getFirstChildIndex(const ChildrenContainer& children) const noexcept
	{
		size_t index(0);
		for (const auto& child : children) {
			if (child) {
				break;
			}
			index++;
		}
		return index;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void CCache::dump() const
	{
		clog << "cache size: " << m_currentSize << endl;

		dumpCache(m_root, move(string()));

		clog << setfill('-') << setw(80) << "-" << endl;

		dumpLinkedList();

		clog << setfill('-') << setw(80) << "-" << endl;
	}

	void CCache::dumpCache(const CCache::CEntryPtr& entry, string name, size_t level/* = 1*/) const
	{
		if (entry->hasProxyValue()) {
			clog << setfill('\t') << setw(level) << "\t" << entry->m_proxyValue << endl;
			name.append(entry->m_proxyValue);
		}

		if (entry->hasData()) {
			clog << setfill('\t') << setw(level) << "\t";
			clog << name << ":" << entry->m_data << endl;
		}

		level++;
		size_t index = 0;
		for (const auto& child : entry->m_children) {
			if (child) {
				auto ch(getChar(index));
				clog << setfill('\t') << setw(level) << "\t" << ch << endl;

				dumpCache(child, move(name + ch), level);
			}
			index++;
		}
	}

	void CCache::dumpLinkedList() const
	{
		auto entry(m_head);
		while (entry) {
			string name;
			getName(entry, name);

			clog << " -> " << name << ": " << entry->m_data << endl;

			entry = entry->m_rSibling;
		};
		clog << endl;
	}

	void CCache::getName(const CEntryPtr& entry, std::string& name) const
	{
		if (entry->hasParent()) {
			getName(entry->m_parent, name);

			// index letter
			name.push_back(getChar(getChildIndex(entry)));
		}
		if (entry->hasProxyValue()) {
			name.append(entry->m_proxyValue);
		}
	}

}