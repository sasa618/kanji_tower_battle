//2次元ベクトル
//位置・速度・力積など、xとyの組はすべてこれで扱う
#pragma once
#include <math.h>  //sqrtf、cosf、sinf用

//円周率
const float pi = 3.14159265f;

//2次元ベクトル
struct VEC2 {
	float x;
	float y;

	VEC2() { x = 0.0f; y = 0.0f; }
	VEC2(float x, float y) { this->x = x; this->y = y; }

	VEC2 operator+(VEC2 v) const { return VEC2(x + v.x, y + v.y); }
	VEC2 operator-(VEC2 v) const { return VEC2(x - v.x, y - v.y); }
	VEC2 operator-() const { return VEC2(-x, -y); }
	VEC2 operator*(float k) const { return VEC2(x * k, y * k); }
	VEC2 operator/(float k) const { return VEC2(x / k, y / k); }

	VEC2& operator+=(VEC2 v) { x += v.x; y += v.y; return *this; }
	VEC2& operator-=(VEC2 v) { x -= v.x; y -= v.y; return *this; }
	VEC2& operator*=(float k) { x *= k; y *= k; return *this; }

	//長さ
	float Length() const { return sqrtf(x * x + y * y); }

	//長さを1にしたもの(長さ0ならそのまま返す)
	VEC2 Normalized() const {
		float len = Length();
		if(len == 0.0f) return *this;

		return VEC2(x / len, y / len);
	}

	//90度回したもの(法線から接線を作るときに使う)
	VEC2 Perpendicular() const { return VEC2(-y, x); }
};

//内積
inline float Dot(VEC2 a, VEC2 b) { return a.x * b.x + a.y * b.y; }

//外積
//2次元なので値は1つ
//回転の向きの判定に使う
inline float Cross(VEC2 a, VEC2 b) { return a.x * b.y - a.y * b.x; }
