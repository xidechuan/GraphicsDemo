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

#ifndef __FONT_MANAGER_H__
#define __FONT_MANAGER_H__

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include "uistructs.h"
#include "vector.h"

namespace gui
{

class FontManager;
class FreeTypeWrapper;
class IFontResource;

class Font
{
	friend class FontManager;
	friend class FreeTypeWrapper;
public:
	Font();
	~Font(){}

	bool isValid() const;
	int getID() const { return m_id; }
	const std::string& getName() const { return m_name; }

	vector2 computeStringSize(const std::wstring& str, size_t offset = 0, size_t len = 0);

private:
	int m_id;
	std::string m_name;
	std::vector<unsigned int> m_charToGlyph;
	struct Glyph
	{
		unsigned short x;
		unsigned short y;
		unsigned char width;
		unsigned char height;
		unsigned char advance;
		char bearingX;
		char bearingY;
		Glyph() : x(0), y(0), width(0), height(0), advance(0), bearingX(0), bearingY(0) {}
	};
	float m_linesDistance;
	std::vector<Glyph> m_glyphs;
	std::weak_ptr<IFontResource> m_resource;
};

class IFontResource
{
public:
	virtual ~IFontResource(){}

	virtual bool createResource(const Font& font, const std::vector<unsigned char>& buffer, size_t width, size_t height) = 0;
};

class FontManager
{
public:
	FontManager(){}
	~FontManager(){}

	bool init();
	void destroy();
	Font createFont(const std::string& fontPath, size_t height);

public:
	std::shared_ptr<FreeTypeWrapper> m_freetype;
};

DECLARE_PTR(FontManager);

}

#endif