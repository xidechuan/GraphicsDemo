/*
 * Copyright (c) 2014 Roman Kuznetsov 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __AUTO_RELEASE_POOL_H__
#define __AUTO_RELEASE_POOL_H__
#ifdef WIN32
    #pragma once
#endif

#include <list>

namespace framework
{

template <typename Releasable>
class AuroreleasePool
{
public:
	AuroreleasePool()
	{
	}

	~AuroreleasePool()
	{
		perform();
	}

	void add(Releasable* ptr)
	{
		m_pool.push_back(ptr);
	}

	void perform()
	{
		for (auto it = m_pool.rbegin(); it != m_pool.rend(); ++it)
		{
			(*it)->Release();
		}
		m_pool.clear();
	}

private:
	std::list<Releasable*> m_pool;
};


}

#endif