#pragma once


#include "math_utils.h"
#include "matrix.h"
#include "vec3.h"


namespace Lumix
{


	class AABB
	{
		public:
			AABB() {}
			AABB(const Vec3& min, const Vec3& max) : m_min(min), m_max(max) {}
			
			const Vec3& getMin() const { return m_min; }
			const Vec3& getMax() const { return m_max; }
			void transform(const Matrix& matrix)
			{
				Vec3 points[8];
				points[0] = m_min;
				points[7] = m_max;
				points[1].set(points[0].x, points[0].y, points[7].z);
				points[2].set(points[0].x, points[7].y, points[0].z);
				points[3].set(points[0].x, points[7].y, points[7].z);
				points[4].set(points[7].x, points[0].y, points[0].z);
				points[5].set(points[7].x, points[0].y, points[7].z);
				points[6].set(points[7].x, points[7].y, points[0].z);

				for(int j = 0; j < 8; ++j)
				{
					points[j] = matrix.multiplyPosition(points[j]); 
				}

				Vec3 new_min = points[0];
				Vec3 new_max = points[0];

				for(int j = 0; j < 8; ++j)
				{
					new_min = minCoords(points[j], new_min);
					new_max = maxCoords(points[j], new_max);
				}

				m_min = new_min;
				m_max = new_max;
			}

		private:
			Vec3 minCoords(const Vec3& a, const Vec3& b)
			{
				return Vec3(
					Math::minValue(a.x, b.x),
					Math::minValue(a.y, b.y),
					Math::minValue(a.z, b.z)
				);
			}


			Vec3 maxCoords(const Vec3& a, const Vec3& b)
			{
				return Vec3(
					Math::maxValue(a.x, b.x),
					Math::maxValue(a.y, b.y),
					Math::maxValue(a.z, b.z)
				);
			}

		private:
			Vec3 m_min;
			Vec3 m_max;
	};


} // namespace Lumix