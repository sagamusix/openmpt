/*
 * ProxyContainer.h
 * ----------------
 * Purpose: A container for proxy representations of containers inside CSoundFile, such as the sample and instrument lists
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include <sol/sol.hpp>

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

struct ProxyContainer
{
	using value_type = sol::object;

	struct iterator
	{
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using value_type = sol::object;
		using reference = sol::object;
		using pointer = sol::object;
		using iterator_category = std::random_access_iterator_tag;

		ProxyContainer &m_container;
		size_type m_index;

		iterator(ProxyContainer &container, size_type index) noexcept : m_container(container), m_index(index) { }
		iterator(const iterator &other, size_type index) noexcept : m_container(other.m_container), m_index(index) { }
		iterator(const iterator&) noexcept = default;
		iterator(iterator&&) noexcept = default;

		iterator operator=(const iterator &other) { MPT_ASSERT(&m_container == &other.m_container); m_index = other.m_index; }
		friend void swap(iterator &lhs, iterator &rhs) { std::swap(lhs.m_index, rhs.m_index); }

		reference operator*() const { return m_container[m_index]; }
		reference operator[](size_type i) const { return m_container[m_index + i]; }

		iterator operator++(int) { return iterator(*this, m_index++); }
		iterator operator--(int) { return iterator(*this, m_index--); }
		iterator operator++() { return iterator(*this, ++m_index); }
		iterator operator--() { return iterator(*this, --m_index); }

		iterator operator+=(size_type i) { m_index += i; return iterator(*this, m_index); }
		iterator operator-=(size_type i) { m_index -= i; return iterator(*this, m_index); }

		friend iterator operator+(const iterator &lhs, size_type i) { return iterator(lhs, lhs.m_index + i); }
		friend iterator operator-(const iterator &lhs, size_type i) { return iterator(lhs, lhs.m_index + i); }
		friend iterator operator+(size_type i, const iterator &rhs) { return iterator(rhs, rhs.m_index + i); }
		friend iterator operator-(size_type i, const iterator &rhs) { return iterator(rhs, rhs.m_index + i); }
		difference_type operator-(const iterator &rhs) { return m_index - rhs.m_index; }

		bool operator==(const iterator &rhs) const { return m_index == rhs.m_index; }
		bool operator!=(const iterator &rhs) const { return m_index != rhs.m_index; }
		bool operator< (const iterator &rhs) const { return m_index < rhs.m_index; }
		bool operator> (const iterator &rhs) const { return m_index > rhs.m_index; }
		bool operator<=(const iterator &rhs) const { return m_index <= rhs.m_index; }
		bool operator>=(const iterator &rhs) const { return m_index >= rhs.m_index; };
	};

	virtual size_t size() { return std::distance(begin(), end()); }
	virtual iterator begin() = 0;
	virtual iterator end() = 0;
	virtual value_type operator[] (size_t index) = 0;
};

}

OPENMPT_NAMESPACE_END
