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

#ifndef __GEOMETRY_3D_H__
#define __GEOMETRY_3D_H__

#include "geometry.h"

namespace framework
{

class Line3D;

class Geometry3D : public Destroyable
{
	friend class Application;

public:
    Geometry3D();
    virtual ~Geometry3D();
	
	bool init(const std::string& fileName);
    
    size_t getMeshesCount() const;
    void renderMesh(size_t index);
	void renderAllMeshes();
	void renderBoundingBox(const matrix44& mvp);

	const bbox3& getBoundingBox() const { return m_boundingBox; }

private:
	GLuint m_vertexArray;
	GLuint m_vertexBuffer;
	GLuint m_indexBuffer;

	geom::Data::Meshes m_meshes;
	size_t m_additionalUVsCount;
	size_t m_verticesCount;
	size_t m_indicesCount;
	bbox3 m_boundingBox;

	bool m_isLoaded;
	std::shared_ptr<Line3D> m_boundingBoxLine;

	virtual void destroy();
};

}

#endif //__GEOMETRY_3D_H__