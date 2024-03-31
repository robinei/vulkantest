#pragma once

#include "Mat4.h"

inline Mat4 unProject
(
    const Vec3 &win, 
    const Mat4 &model, 
    const Mat4 &proj, 
    const Vec4 &viewport
)
{
    detail::tmat4x4<T> inverse = glm::inverse(proj * model);

    detail::tvec4<T> tmp = detail::tvec4<T>(win, T(1));
    tmp.x = (tmp.x - T(viewport[0])) / T(viewport[2]);
    tmp.y = (tmp.y - T(viewport[1])) / T(viewport[3]);
    tmp = tmp * T(2) - T(1);

    detail::tvec4<T> obj = inverse * tmp;
    obj /= obj.w;

    return detail::tvec3<T>(obj);
}