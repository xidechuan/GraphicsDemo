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

#pragma once
#include "structs.h"
#include "destroyable.h"
#include <vector>

namespace framework
{

class UniformBuffer : public Destroyable
{
	friend class Application;

public:
	UniformBuffer();
	virtual ~UniformBuffer();

	D3D11_BUFFER_DESC getDefaultConstant(unsigned int size);
	D3D11_BUFFER_DESC getDefaultStructured(unsigned int size, unsigned int structsize);

	template<typename DataType> bool initImmutable(const Device& device, const std::vector<DataType>& data, const D3D11_BUFFER_DESC& desc)
	{
		destroy();
		m_desc = desc;
		m_desc.Usage = D3D11_USAGE_IMMUTABLE;
		m_desc.CPUAccessFlags = 0;
		initBufferImmutable(device, data.data(), sizeof(DataType), count);

		if (m_buffer != 0) initDestroyable();
		return m_buffer != 0;
	}

	template<typename DataType> bool initDefaultConstant(const Device& device)
	{
		return init<DataType>(device, 1, getDefaultConstant(sizeof(DataType)));
	}

	template<typename DataType> bool initDefaultStructured(const Device& device, size_t count)
	{
		return init<DataType>(device, count, getDefaultStructured(count, sizeof(DataType)));
	}

	template<typename DataType> bool init(const Device& device, size_t count, const D3D11_BUFFER_DESC& desc)
	{
		destroy();
		m_desc = desc;	
		initBuffer(device, sizeof(DataType), count);

		if (m_buffer != 0) initDestroyable();
		return m_buffer != 0;
	}

	template<typename DataType> const DataType& getData() const
	{
		return getElement<DataType>(0);
	}

	template<typename DataType> const DataType& getElement(int index) const
	{
		static DataType dummy;
		size_t i = index * sizeof(DataType);
		if (i < 0 || i >= m_bufferInMemory.size()) return dummy;
		return reinterpret_cast<const DataType&>(m_bufferInMemory[i]);
	}

	template<typename DataType> void setData(const DataType& value)
	{
		setElement<DataType>(0, value);
	}

	template<typename DataType> void setElement(int index, const DataType& value)
	{
		size_t i = index * sizeof(DataType);
		if (i < 0 || i >= m_bufferInMemory.size()) return;
		DataType& elem = reinterpret_cast<DataType&>(m_bufferInMemory[i]);
		elem = value;
		m_isChanged = true;
	}

	template<typename DataType> int size() const
	{
		return m_bufferInMemory.size() / sizeof(DataType);
	}

	unsigned int sizeInBytes() const
	{
		return (unsigned int)m_bufferInMemory.size();
	}

	ID3D11Buffer* getBuffer()
	{
		return m_buffer;
	}

	void applyChanges(const Device& device);

private:
	virtual void destroy();
	void initBuffer(const Device& device, size_t elemSize, size_t count);
	void initBufferImmutable(const Device& device, unsigned char* dataPtr, size_t elemSize, size_t count);

	ID3D11Buffer* m_buffer;
	D3D11_BUFFER_DESC m_desc;

	std::vector<unsigned char> m_bufferInMemory;
	bool m_isChanged;
};

}