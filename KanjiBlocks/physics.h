//剛体の物理(落下と衝突)
//参考資料はREADME.mdの「参考資料」を参照
#pragma once
#include <windows.h>
#include "vec2.h"

//1つの部品(凸多角形)の頂点数の上限(字の画は四角形)
const int maxPointCnt = 4;
//1つの物体を作る部品の数の上限
//字は画1本が部品1つ
//森は木の画の3倍
//エディタで画を足せるように余裕を持たせる
const int maxPartCnt = 24;
//物体の数の上限(壁を含む)
const int maxBodyCnt = 512;

//重力加速度(px/s^2)、下が正
//100px=1mとして9.8m/s^2に相当
const VEC2 gravity = VEC2(0.0f, 980.0f);
//1フレームを何回に分けて計算するか
const int subStepCnt = 2;
//全ペアの押し出しを何周するか
const int positionIterations = 2;
//全接触点へのインパルスを何周するか
const int velocityIterations = 10;

//許容貫通誤差
//このぶんのめり込みは残しておく(毎コマ接触点が取れて、支える力が安定する)(px)
const float slop = 0.5f;

//反発係数
const float restitution = 0.0f;
//これより遅くぶつかったときは跳ねない(細かい跳ねを防ぐ)(px/s)
const float bounceSpeed = 100.0f;
//摩擦係数(摩擦力の上限 = 摩擦係数 × 垂直抗力)
const float friction = 0.8f;
//同じ高さの頂点とみなす誤差(px)
const float contactEpsilon = 0.5f;

//これより遅いフレームが続いたら「静止中」とみなす(表示に使う)
const float restSpeed = 10.0f;
const float restAngularSpeed = 0.1f;
const int restFrameCnt = 40;

//物体を作る部品(凸多角形)
//凹んだ形は凸多角形を組み合わせて作る
struct S_PART {
	VEC2 localPoints[maxPointCnt];  //物体の重心から見た回転前の頂点
	int pointCnt;  //頂点数

	S_PART() { pointCnt = 0; }
};

//剛体
//凸多角形の部品をいくつか組み合わせて1つの形にする
//質量の逆数が0のものは壁として扱い、動かさない
class BODY {
private:
	int id;  //通し番号(物体を消して並びが詰まっても変わらない)

	VEC2 center;  //重心の位置
	float angle;  //回転角(ラジアン)
	VEC2 velocity;  //速度(px/s)
	float angularVelocity;  //角速度(rad/s)

	float invMass;  //質量の逆数(0なら壁)
	float invInertia;  //慣性モーメントの逆数

	S_PART parts[maxPartCnt];  //部品
	int partCnt;  //部品の数

	UINT rgb;  //色
	int restCnt;  //ほぼ止まっているフレームの数

	VEC2 boundingBoxLT;  //頂点を囲む長方形の左上(ざっくり判定用)
	VEC2 boundingBoxRB;  //頂点を囲む長方形の右下

public:
	BODY() {
		id = 0;
		angle = 0.0f;
		angularVelocity = 0.0f;
		invMass = 0.0f;
		invInertia = 0.0f;
		partCnt = 0;
		rgb = 0;
		restCnt = 0;
	}

	int GetID() { return id; }

	VEC2 GetCenter() { return center; }
	float GetAngle() { return angle; }
	VEC2 GetVelocity() { return velocity; }
	float GetAngularVelocity() { return angularVelocity; }

	float GetInvMass() { return invMass; }
	float GetInvInertia() { return invInertia; }

	int GetPartCnt() { return partCnt; }
	int GetPointCnt(int part) { return parts[part].pointCnt; }

	UINT GetRGB() { return rgb; }
	bool IsStatic() { return invMass == 0.0f; }

	VEC2 GetBoundingBoxLT() { return boundingBoxLT; }
	VEC2 GetBoundingBoxRB() { return boundingBoxRB; }

	void SetID(int id) { this->id = id; }
	void SetVelocity(VEC2 v) { velocity = v; }
	void SetAngularVelocity(float w) { angularVelocity = w; }
	void SetAngle(float a) { angle = a; }
	void MoveCenterPos(VEC2 delta) { center += delta; }

	//空の物体にする(部品を足す前に呼ぶ)
	void Initialize(VEC2 center, float angle, UINT rgb) {
		this->center = center;
		this->angle = angle;
		this->rgb = rgb;

		velocity = VEC2(0.0f, 0.0f);
		angularVelocity = 0.0f;
		invMass = 0.0f;
		invInertia = 0.0f;
		partCnt = 0;
		restCnt = 0;
	}

	//部品(凸多角形)を足す
	//points: 物体の中心から見た頂点を順に並べたもの(時計回りでも反時計回りでもよい)
	//戻り値: 足せないとき(部品が多すぎるか、頂点数が範囲外)はfalse
	bool AddPart(const VEC2* points, int pointCnt) {
		if(partCnt >= maxPartCnt || pointCnt < 3 || pointCnt > maxPointCnt) return false;

		S_PART* part = &parts[partCnt];
		for(int i=0; i<pointCnt; i++) part->localPoints[i] = points[i];
		part->pointCnt = pointCnt;
		partCnt++;


		return true;
	}

	//部品を全部足したあとに呼ぶ
	//重心を求めて原点にそろえ、質量と慣性モーメントを計算する
	//部品を三角形に分け、三角形の面積(=質量、密度1)・重心・慣性モーメントを足し合わせる
	//戻り値: ずらす前の座標で見た重心
	VEC2 CalculateMass() {
		//全体の質量と重心(三角形の重心を、面積で重みづけして平均する)
		float mass = 0.0f;
		VEC2 centroid;

		for(int p=0; p<partCnt; p++) {
			S_PART* part = &parts[p];

			for(int i=1; i<part->pointCnt - 1; i++) {  //頂点0を中心にした扇形に分ける
				VEC2 a = part->localPoints[0];
				VEC2 b = part->localPoints[i];
				VEC2 c = part->localPoints[i + 1];

				float m = fabsf(Cross(b - a, c - a)) / 2.0f;  //三角形の面積
				mass += m;
				centroid += (a + b + c) / 3.0f * m;
			}
		}

		if(mass == 0.0f) return centroid;

		centroid = centroid / mass;

		//重心が原点になるように頂点をずらす
		for(int p=0; p<partCnt; p++) {
			for(int i=0; i<parts[p].pointCnt; i++) parts[p].localPoints[i] -= centroid;
		}

		//慣性モーメント
		//三角形の慣性モーメントm/12 × (|r1|^2 + |r2|^2 + |r3|^2)に、平行軸の定理の分m × d^2を足す
		//(r1～r3は三角形の重心から見た頂点、dは三角形の重心から物体の重心までの距離)
		float inertia = 0.0f;

		for(int p=0; p<partCnt; p++) {
			S_PART* part = &parts[p];

			for(int i=1; i<part->pointCnt - 1; i++) {
				VEC2 a = part->localPoints[0];
				VEC2 b = part->localPoints[i];
				VEC2 c = part->localPoints[i + 1];

				float m = fabsf(Cross(b - a, c - a)) / 2.0f;
				VEC2 g = (a + b + c) / 3.0f;  //三角形の重心(物体の重心から見た位置なので、|g|がd)

				VEC2 r1 = a - g;
				VEC2 r2 = b - g;
				VEC2 r3 = c - g;

				inertia += m / 12.0f * (Dot(r1, r1) + Dot(r2, r2) + Dot(r3, r3)) + m * Dot(g, g);
			}
		}

		invMass = 1.0f / mass;
		invInertia = 1.0f / inertia;

		UpdateBoundingBox();

		return centroid;
	}

	//壁にする(動かない)
	void SetStatic() {
		invMass = 0.0f;
		invInertia = 0.0f;
	}

	//部品partの、回転を反映した頂点の位置
	//output: 頂点数ぶんの配列
	void GetWorldPoints(int part, VEC2* output) {
		float c = cosf(angle);
		float s = sinf(angle);

		for(int i=0; i<parts[part].pointCnt; i++) {
			VEC2 p = parts[part].localPoints[i];
			output[i] = VEC2(center.x + p.x * c - p.y * s, center.y + p.x * s + p.y * c);
		}
	}

	//点pointの速度(回転の分も含む)
	VEC2 GetVelocityAt(VEC2 point) {
		return velocity + (point - center).Perpendicular() * angularVelocity;
	}

	//バウンディングボックスを更新
	void UpdateBoundingBox() {
		boundingBoxLT = center;
		boundingBoxRB = center;

		VEC2 points[maxPointCnt];
		for(int p=0; p<partCnt; p++) {
			GetWorldPoints(p, points);

			for(int i=0; i<parts[p].pointCnt; i++) {
				boundingBoxLT.x = min(boundingBoxLT.x, points[i].x);
				boundingBoxLT.y = min(boundingBoxLT.y, points[i].y);
				boundingBoxRB.x = max(boundingBoxRB.x, points[i].x);
				boundingBoxRB.y = max(boundingBoxRB.y, points[i].y);
			}
		}
	}

	//バウンディングボックスが重なっているか(細かい判定の前のふるい)
	bool IsBoundingBoxOverlayed(BODY* other) {
		VEC2 aLT = boundingBoxLT;
		VEC2 aRB = boundingBoxRB;
		VEC2 bLT = other->GetBoundingBoxLT();
		VEC2 bRB = other->GetBoundingBoxRB();

		return aLT.x <= bRB.x && aRB.x >= bLT.x  //X軸について重なっているか
			&& aLT.y <= bRB.y && aRB.y >= bLT.y;  //Y軸について重なっているか
	}

	//点pointにインパルス(力積)を与える(壁には効かない)
	void ApplyImpulse(VEC2 point, VEC2 impulse) {
		if(IsStatic()) return;

		velocity += impulse * invMass;
		angularVelocity += Cross(point - center, impulse) * invInertia;
	}

	//重力で位置と回転を更新する
	//dt: 前回の更新からの経過時間
	void UpdatePhysics(float dt) {
		if(IsStatic()) return;

		velocity += gravity * dt;
		center += velocity * dt;
		angle += angularVelocity * dt;
	}

	//ほぼ止まっているフレーム数を数える
	void UpdateRestCnt() {
		if(velocity.Length() < restSpeed && fabsf(angularVelocity) < restAngularSpeed) restCnt++;
		else restCnt = 0;
	}

	//静止中とみなせるか
	bool IsResting() { return restCnt >= restFrameCnt; }
};

//物体(壁を含む)と、その数
extern BODY bodies[maxBodyCnt];
extern int bodyCnt;

void ClearBodies();
BODY* AddBody(VEC2 center, float angle, UINT rgb);
BODY* AddWall(VEC2 center, float w, float h, UINT rgb);
void RemoveBody(int idx);
int FindBodyIdx(int id);

void StepPhysics(float dt);

int GetContactCnt();
void GetContactPair(int contact, int* a, int* b);
