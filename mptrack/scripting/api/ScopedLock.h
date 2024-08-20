/*
 * ScopedLock.h
 * ------------
 * Purpose: A wrapper class for CSoundFile and CModDoc that ensures that access to those objects is done while a lock is held.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "../../../soundlib/AudioCriticalSection.h"
#include "../../Moddoc.h"

OPENMPT_NAMESPACE_BEGIN

template<typename T>
struct ScopedLock
{
	T &m_object;
	CriticalSection m_cs;

	ScopedLock(T &object, CriticalSection &&cs) noexcept : m_object{object}, m_cs{std::move(cs)} { }
	ScopedLock(ScopedLock<typename std::remove_const<T>::type> &&other) noexcept : m_object{other.m_object}, m_cs{std::move(other.m_cs)} { }
	template<typename Tother>
	ScopedLock(T &object, ScopedLock<Tother> &&lockFrom) noexcept : m_object{object}, m_cs{std::move(lockFrom.m_cs)} { }

	operator const CriticalSection& () const noexcept { return m_cs; }

	operator T& () noexcept { return m_object; }
	operator const T& () const noexcept { return m_object; }

	operator T* () noexcept { return &m_object; }
	operator const T* () const noexcept { return &m_object; }

	T* operator-> () noexcept { return &m_object; }
	const T* operator-> () const noexcept { return &m_object; }

	bool operator==(const ScopedLock<const T> &other) const noexcept { return &m_object == &other.m_object; }
	bool operator!=(const ScopedLock<const T> &other) const noexcept { return &m_object != &other.m_object; }
};

using CModDocLock = ScopedLock<CModDoc>;
using CModDocLockConst = ScopedLock<const CModDoc>;

template<typename SndFileType>
struct CSoundFileLockT : public ScopedLock<SndFileType>
{
	CSoundFileLockT(CModDocLock &&modDocLock) noexcept : ScopedLock<SndFileType>(modDocLock->GetSoundFile(), std::move(modDocLock.m_cs)) { }
	CSoundFileLockT(CModDocLockConst &&modDocLock) noexcept : ScopedLock<SndFileType>(modDocLock->GetSoundFile(), std::move(modDocLock.m_cs)) { }
	CSoundFileLockT(CSoundFileLockT<SndFileType> &&other) noexcept : ScopedLock<SndFileType>(other.m_sndFile, std::move(other.m_cs)) { }
};

using CSoundFileLock = CSoundFileLockT<CSoundFile>;
using CSoundFileLockConst = CSoundFileLockT<const CSoundFile>;

template<typename T>
ScopedLock<T> make_lock(T &object, CriticalSection &&cs) noexcept
{
	return ScopedLock<T>(object, std::move(cs));
}
template<typename T, typename Tother>
ScopedLock<T> make_lock(T &object, ScopedLock<Tother> &&lockFrom) noexcept
{
	return ScopedLock<T>(object, std::move(lockFrom.m_cs));
}

OPENMPT_NAMESPACE_END
