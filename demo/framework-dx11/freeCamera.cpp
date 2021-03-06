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

#include "stdafx.h"
#include "freeCamera.h"

namespace framework
{

FreeCamera::FreeCamera():
	m_moveForward(false),
	m_moveBackward(false),
	m_moveLeft(false),
	m_moveRight(false),
	m_rotationMode(false),
	m_speed(50.0f),
	m_updateTime(0)
{

}

void FreeCamera::initWithPositionDirection(int width, int height, const vector3& from, const vector3& to)
{
	init(width, height);
	setPosition(from);
	
	vector3 dir = to - from;
	dir.norm();
	quaternion q;
	q.set_from_axes(vector3(0, 0, 1), dir);
	setOrientation(q);
	matrix33 m(q);
	vector3 angles = m.to_euler();

	m_angles.x = n_rad2deg(angles.y);
	m_angles.y = n_rad2deg(angles.x);
}

void FreeCamera::onKeyButton(int key, int scancode, bool pressed)
{
	if (key == InputKeys::Scan::W || key == InputKeys::Scan::ArrowUp)
	{
		m_moveForward = pressed;
	}
	else if (key == InputKeys::Scan::S || key == InputKeys::Scan::ArrowDown)
	{
		m_moveBackward = pressed;
	}
	else if (key == InputKeys::Scan::A || key == InputKeys::Scan::ArrowLeft)
	{
		m_moveLeft = pressed;
	}
	else if (key == InputKeys::Scan::D || key == InputKeys::Scan::ArrowRight)
	{
		m_moveRight = pressed;
	}
}
	
void FreeCamera::onMouseButton(double xpos, double ypos, int button, bool pressed)
{
	if (button == InputKeys::MouseButton::LeftButton)
	{
		if (pressed)
		{ 
			m_rotationMode = true;
			m_lastMousePosition.x = (float)xpos;
			m_lastMousePosition.y = (float)ypos;
			m_currentMousePosition = m_lastMousePosition;
			m_updateTime = 0;
		}
		else m_rotationMode = false;
	}
}
	
void FreeCamera::onMouseMove(double xpos, double ypos)
{
	if (m_rotationMode)
	{
		m_currentMousePosition = vector2((float)xpos, (float)ypos);
	}
}

void FreeCamera::update(double elapsedTime)
{
	if (m_rotationMode)
	{
		m_updateTime += elapsedTime;
		if (m_updateTime >= 1.0f / 60.0f)
		{
			vector2 delta = m_currentMousePosition - m_lastMousePosition;
			if (fabs(delta.x) > 1e-5 || fabs(delta.y) > 1e-5)
			{
				float ang = this->m_camera.GetAngleOfView() * 0.5f;
				float w = this->m_camera.GetNearPlane() * n_tan(n_deg2rad(ang));
				float h = w / this->m_camera.GetAspectRatio();

				vector2 screenSize = Application::instance()->getScreenSize();
				float ax = n_rad2deg(atan2(w * (2.0f * delta.x / screenSize.x), this->m_camera.GetNearPlane()));
				float ay = n_rad2deg(atan2(h * (2.0f * delta.y / screenSize.y), this->m_camera.GetNearPlane()));

				vector2 old_angles = m_angles;
				m_angles.x -= float(ax * 100.0 * m_updateTime);
				m_angles.y += float(ay * 100.0 * m_updateTime);
				if (m_angles.y > 89.9f) m_angles.y = 89.9f;
				if (m_angles.y < -89.9) m_angles.y = -89.9f;

				if (!old_angles.isequal(m_angles, 0.00001f))
				{
					quaternion q1;
					q1.set_rotate_axis_angle(vector3(0, 1, 0), n_deg2rad(m_angles.x));
					quaternion q2;
					q2.set_rotate_axis_angle(vector3(1, 0, 0), n_deg2rad(m_angles.y));
					m_orientation = q1 * q2;
				}

				m_lastMousePosition = m_currentMousePosition;
			}

			m_updateTime -= 1.0f / 60.0f;
		}
	}

	if (m_moveForward)
	{
		m_position += (m_orientation.z_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveBackward)
	{
		m_position -= (m_orientation.z_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveLeft)
	{
		m_position += (m_orientation.x_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveRight)
	{
		m_position -= (m_orientation.x_direction() * m_speed * (float)elapsedTime);
	}

	//utils::Logger::toLogWithFormat("%.2f, %.2f, %.2f\n", m_position.x, m_position.y, m_position.z);
}

}