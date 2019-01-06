#include <cmath>

struct v2
{
    float x, y;

    v2 () {}
    v2(const float a, const float b)
    {
        x = a;
        y = b;
    }

    void operator += (const v2 a)
    {
        x += a.x;
        y += a.y;
    }

    void operator -= (const v2 a)
    {
        x -= a.x;
        y -= a.y;
    }

    void operator *= (const float b)
    {
        x *= b;
        y *= b;
    }

    void operator /= (const float b)
    {
        if (b < 0.000000001f)
            return;

        x /= b;
        y /= b;
    }
};

struct v3
{
    float x, y, z;

    v3 () {}
    v3(const float a, const float b, const float c)
    {
        x = a;
        y = b;
        z = c;
    }

    void operator += (const v3 a)
    {
        x += a.x;
        y += a.y;
        z += a.z;
    }

    void operator -= (const v3 a)
    {
        x -= a.x;
        y -= a.y;
        z -= a.z;
    }

    void operator *= (const float b)
    {
        x *= b;
        y *= b;
        z *= b;
    }

    void operator /= (const float b)
    {
        if (b < 0.000000001f)
            return;

        x /= b;
        y /= b;
        z /= b;
    }
};

v2 operator + (const v2 a, const v2 b)
{
    v2 res = a;
    res += b;

    return res;
}

v2 operator - (const v2 a, const v2 b)
{
    v2 res = a;
    res -= b;

    return res;
}

v2 operator * (const v2 a, const float b)
{
    v2 res = a;
    res *= b;

    return res;
}

v2 operator / (v2 a, const float b)
{
    v2 res = a;
    res /= b;

    return res;
}

v3 operator + (const v3 a, const v3 b)
{
    v3 res = a;
    res += b;

    return res;
}

v3 operator - (const v3 a, const v3 b)
{
    v3 res = a;
    res -= b;

    return res;
}

v3 operator * (const v3 a, const float b)
{
    v3 res = a;
    res *= b;

    return res;
}

v3 operator / (v3 a, const float b)
{
    v3 res = a;
    res /= b;

    return res;
}

v3 cross(const v3 a, const v3 b)
{
    v3 res;
    res.x = a.y * b.z - a.z * b.y;
    res.y = a.x * b.z - a.z * b.x;
    res.z = a.x * b.y - a.y * b.x;

    return res;
}

float dot(const v3 a, const v3 b)
{
    float res;
    res = a.x * b.x + a.y * b.y + a.z * b.z;

    return res;
}

float dot(const v2 a, const v2 b)
{
    float res;
    res = a.x * b.x + a.y * b.y;

    return res;
}

float length(const v2 a)
{
    float res;
    res = sqrt(a.x * a.x + a.y * a.y);

    return res;
}

float length(const v3 a)
{
    float res;
    res = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);

    return res;
}

v2 normalize(v2 a)
{
    float len = length(a);
    if (len < 0.000000001f)
        return a;

    a.x /= len;
    a.y /= len;

    return a;
}

v3 normalize(v3 a)
{
    float len = length(a);
    if (len < 0.0001f)
        return v3(0, 0, 0);

    a.x /= len;
    a.y /= len;
    a.z /= len;

    return a;
}

v3 hadamard(const v3& a, const v3& b)
{
    return v3(a.x * b.x, a.y * b.y, a.z * b.z);
}

v3 lerp(const v3& a, const v3& b, const float val)
{
    return a * (1 - val) + b * val;
}
