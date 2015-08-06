#pragma once

class SizeI;
class PointI;
class RectI;

static const double D_PI = 3.14159265358979323846;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline double ToRadian(double theta) 
{
	return theta * (D_PI / 180.0);
}

inline double ToDegrees(double theta) 
{
	return theta / (D_PI / 180.0);
}

inline int ClampI(const int &v, const int &l, const int &h)
{
    if (h <= l) return l;
    return v < l ? l : (v > h ? h : v);
}

class SizeI : public SIZE
{
public:

	SizeI()
	{
		cx = 0;
		cy = 0;
	}

	SizeI(int initCX, int initCY)
	{
		cx = initCX;
		cy = initCY;
	}

	SizeI(const SIZE &initSize)
	{
		*(SIZE*)this = initSize;
	}

	SizeI(const POINT &initPt)
	{
		*(POINT*)this = initPt;
	}

	void Set(int initCX, int initCY)
	{
		cx = initCX;
		cy = initCY;
	}

	bool operator ==(const SIZE &size) const
	{
		return (cx == size.cx && cy == size.cy);
	}

	bool operator !=(const SIZE &size) const
	{
		return (cx != size.cx || cy != size.cy);
	}

	inline bool IsEmpty() const
	{
		return cx == 0 && cy == 0;
	}	

	SizeI operator +(const SIZE &size) const
	{
		return SizeI(cx + size.cx, cy + size.cy);
	}

	SizeI operator -(const SIZE &size) const
	{
		return SizeI(cx - size.cx, cy - size.cy);
	}

	SizeI operator -() const
	{
		return SizeI(-cx, -cy);
	}

	PointI operator +(const POINT &point) const;
	PointI operator -(const POINT &point) const;
};

class PointI : public POINT
{
public:

	PointI()
	{
		x = 0;
		y = 0;
	}

	PointI(int initX, int initY)
	{
		x = initX;
		y = initY;
	}

	PointI(const POINT &initPt)
	{
		*(POINT*)this = initPt;
	}

	PointI(const SIZE &initSize)
	{
		*(SIZE*)this = initSize;
	}

	bool operator ==(const POINT &point) const
	{
		return x == point.x && y == point.y;
	}

	bool operator !=(const POINT &point) const
	{
		return x != point.x || y != point.y;
	}

	PointI operator +(const SIZE &size) const
	{
		return PointI(x + size.cx, y + size.cy);
	}

	PointI operator -(const SIZE &size) const
	{
		return PointI(x - size.cx, y - size.cy);
	}

	PointI operator -() const
	{
		return PointI(-x, -y);
	}

	PointI operator +(const POINT &point) const
	{
		return PointI(x + point.x, y + point.y);
	}

	SizeI operator -(const POINT &point) const
	{
		return SizeI(x - point.x, y - point.y);
	}

	inline PointI Clamp(const RECT &limit) const
	{
		return PointI(ClampI(x, limit.left, limit.right), ClampI(y, limit.top, limit.bottom));
	}
};

class RectI : public RECT
{
public:
	// Constructors
	RectI()
	{
		left = 0;
		top = 0;
		right = 0;
		bottom = 0;
	}

	RectI(int l, int t, int r, int b)
	{
		left = l;
		top = t;
		right = r;
		bottom = b;
	}

	RectI(const RECT& other)
	{
		Copy(&other);
	}

	RectI(LPCRECT other)
	{
		Copy(other);
	}

	RectI(const POINT &point, const SIZE &size)
	{
		right = (left = point.x) + size.cx;
		bottom = (top = point.y) + size.cy;
	}

	RectI(const POINT &topLeft, const POINT &bottomRight)
	{
		left = topLeft.x;
		top = topLeft.y;
		right = bottomRight.x;
		bottom = bottomRight.y;
	}

	inline void Copy(LPCRECT other)
	{
		left = other->left;
		top = other->top;
		right = other->right;
		bottom = other->bottom;
	}

	inline int Width() const
	{
		return right - left;
	}

	inline int Height() const
	{
		return bottom - top;
	}

	inline SizeI Size() const
	{
		return SizeI(right - left, bottom - top);
	}

	inline PointI TopLeft() const
	{
		return PointI(left, top);
	}

	inline PointI BottomRight() const
	{
		return PointI(right, bottom);
	}

	inline PointI Center() const
	{
		return PointI((left + right) / 2, (top + bottom) / 2);
	}

	inline operator LPRECT()
	{
		return this;
	}

	inline operator LPCRECT() const
	{
		return this;
	}

	inline bool IsEmpty() const
	{
		return left >= right || top >= bottom;
	}

	inline bool IsNull() const
	{
		return left == 0 && right == 0 && top == 0 && bottom == 0;
	}

	inline bool Contains(const POINT &point) const
	{
		return left <= point.x && right >= point.x && top <= point.y && bottom >= point.y;
	}

	inline void Clear()
	{
		left = right = top = bottom = 0;
	}

	inline void Set(int x1, int y1, int x2, int y2)
	{
		left = x1;
		top = y1;
		right = x2;
		bottom = y2;
	}

	inline void Set(const POINT &topLeft, const POINT &bottomRight)
	{
		Set(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
	}

	inline RectI Inflate(int xy) const
	{
		return RectI(left - xy, top - xy, right + xy, bottom + xy);
	}

	inline RectI Inflate(int x, int y) const
	{
		return RectI(left - x, top - y, right + x, bottom + y);
	}

	inline RectI Inflate(const SizeI &s) const
	{
		return RectI(left - s.cx, top - s.cy, right + s.cx, bottom + s.cy);
	}

	inline bool Intersects(const RectI &other) const
	{
		return left < other.right &&
			top < other.bottom &&
			right > other.left &&
			bottom > other.top;
	}	

	inline RectI Intersection(const RectI &other) const
	{
		if (!Intersects(other)) return RectI();
        return RectI(std::max(left, other.left), std::max(top, other.top), std::min(right, other.right), std::min(bottom, other.bottom));
	}	

	inline RectI Union(const RectI &other) const
	{
		if (IsEmpty()) return other;
		if (other.IsEmpty()) return this;
        return RectI(std::min(left, other.left), std::min(top, other.top), std::max(right, other.right), std::max(bottom, other.bottom));
	}	

	inline RectI Clamp(const RectI &limit) const
	{
		SizeI offset(0,0);

		if (top < limit.top)  
			offset.cy = limit.top - top;

		if (left < limit.left)  
			offset.cx = limit.left - left;

		if (bottom > limit.bottom)  
			offset.cy = limit.bottom - bottom;

		if (right > limit.right)  
			offset.cx = limit.right - right;

		return Offset(offset);
	}

	inline RectI Crop(const RectI &limit) const
	{
		RectI result(this);

		if (top < limit.top)  result.top = limit.top;
		if (left < limit.left) result.left = limit.left;
		if (bottom > limit.bottom) result.bottom = limit.bottom;
		if (right > limit.right) result.right = limit.right;

		return result;
	}

	inline RectI Offset(const PointI &pt) const
	{
		return RectI(left + pt.x, top + pt.y, right + pt.x, bottom + pt.y);
	}	

	inline RectI Offset(int x, int y) const
	{
		return RectI(left + x, top + y, right + x, bottom + y);
	}

	inline void operator =(const RECT& other)
	{
		Copy(&other);
	}

	inline bool operator ==(const RECT& other) const
	{
		return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
	}

	inline bool operator !=(const RECT& other) const
	{
		return left != other.left || top != other.top || right != other.right || bottom != other.bottom;
	}

    operator Gdiplus::Rect() const { return Gdiplus::Rect(left, top, Width(), Height()); };
};


inline PointI SizeI::operator +(const POINT &point) const
{ 
	return PointI(cx + point.x, cy + point.y); 
}

inline PointI SizeI::operator -(const POINT &point) const
{ 
	return PointI(cx - point.x, cy - point.y); 
}

inline RectI CenterRect(const SizeI &s, int xx, int yy)
{
	auto x = xx - (s.cx / 2);
	auto y = yy - (s.cy / 2);
	return RectI(x, y, x + s.cx, y + s.cy);
}

inline RectI CenterRect(const SizeI &s, const RectI &limit)
{
	auto center = limit.Center();
	return CenterRect(s, center.x, center.y);
}

inline RectI CenterRect(const SizeI &s, const SizeI &limit)
{
	return CenterRect(s, limit.cx / 2, limit.cy / 2);
}

inline RectI CenterRect(const SizeI &s, const PointI &limit)
{
	return CenterRect(s, limit.x, limit.y);
}

inline RectI CenterRect(const RectI &r, const RectI &limit)
{
	return CenterRect(r.Size(), limit);
}

