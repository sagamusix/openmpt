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

template<typename T, std::ptrdiff_t IndexAdjustment = -1>
struct ProxyContainer
{
	using value_type = T;

	struct iterator
	{
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using value_type = T;
		using reference = T;//T&;
		using pointer = void;
		using const_reference = typename std::add_const<reference>::type;
		using iterator_category = std::random_access_iterator_tag;

		ProxyContainer &m_container;
		size_type m_index;

		iterator(ProxyContainer &container, size_type index) noexcept : m_container{container}, m_index{index} { }
		iterator(const iterator &other, size_type index) noexcept : m_container{other.m_container}, m_index{index} { }
		iterator(const iterator&) noexcept = default;
		iterator(iterator&&) noexcept = default;

		iterator operator=(const iterator &other) { MPT_ASSERT(&m_container == &other.m_container); m_index = other.m_index; }
		friend void swap(iterator &lhs, iterator &rhs) { std::swap(lhs.m_index, rhs.m_index); }

		reference operator*() { return m_container[m_index]; }
		reference operator[](size_type i) { return m_container[m_index + i]; }
		const_reference operator*() const { return m_container[m_index]; }
		const_reference operator[](size_type i) const { return m_container[m_index + i]; }

		iterator operator++(int) { return iterator{*this, m_index++}; }
		iterator operator--(int) { return iterator{*this, m_index--}; }
		iterator operator++() { return iterator{*this, ++m_index}; }
		iterator operator--() { return iterator{*this, --m_index}; }

		iterator operator+=(size_type i) { m_index += i; return iterator{*this, m_index}; }
		iterator operator-=(size_type i) { m_index -= i; return iterator{*this, m_index}; }

		friend iterator operator+(const iterator &lhs, size_type i) { return iterator{lhs, lhs.m_index + i}; }
		friend iterator operator-(const iterator &lhs, size_type i) { return iterator{lhs, lhs.m_index + i}; }
		friend iterator operator+(size_type i, const iterator &rhs) { return iterator{rhs, rhs.m_index + i}; }
		friend iterator operator-(size_type i, const iterator &rhs) { return iterator{rhs, rhs.m_index + i}; }
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

	static constexpr auto index_adjustment() noexcept { return IndexAdjustment; }
};

// sol::usertype_container<Container> can inherit from UsertypeContainer<Container> for common containers
template<typename TContainer>
struct UsertypeContainer
{
	static int size(lua_State *L)
	{
		auto &self = sol::stack::get<TContainer &>(L, 1);
		return sol::stack::push(L, self.size());
	}
	static auto begin(lua_State *, TContainer &that)
	{
		return that.begin();
	}
	static auto end(lua_State *, TContainer &that)
	{
		return that.end();
	}
	static int set(lua_State *L)
	{
		auto &that = sol::stack::get<TContainer &>(L, 1);
		auto index = sol::stack::get<size_t>(L, 2);
		sol::stack_object value = sol::stack_object{L, sol::raw_index{3}};
		if(sol::type_of(L, 3) == sol::type::lua_nil)
			that.erase(typename TContainer::iterator{that, index});
		else
			that.insert(typename TContainer::iterator{that, index}, value.as<typename TContainer::value_type>());
		return 0;
	}
	static int index_set(lua_State *L)
	{
		return set(L);
	}

	static std::ptrdiff_t index_adjustment(lua_State *, TContainer &)
	{
		return TContainer::index_adjustment();
	}
};


}  // namespace Scripting

OPENMPT_NAMESPACE_END
