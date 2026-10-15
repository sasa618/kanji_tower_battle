//剛体の物理(落下と衝突)
//1フレームの流れ(README.mdの「参考資料」の「処理の流れ」を参照)
//1. 重力で速度と位置を進める
//2. 重なった物体を押し出して、接触点を集める
//調べる組は、物体を上の端の高さの順に並べて絞り込む(平面走査)
//押し出す向きは、分離軸判定で重なりがいちばん浅かった向き
//3. 接触点ごとにインパルス(力積)を与えて、近づく速度を打ち消す
//これを全接触点に対して何周も繰り返す(逐次インパルス法)
//前回のインパルスを最初に与えておく(ウォームスタート)と、少ない周回で積み重ねが落ち着く
//前回の接触点との対応は、同じ順に並んだ2つの列を先頭から同時になぞって取る(尺取り法)
#include <float.h>  //FLT_MAX用
#include <algorithm>  //std::sort用
#include "physics.h"

//接触点
//押し出しのときに集めて、インパルスの計算に使う
struct S_CONTACT {
	int a;  //ぶつかった物体の番号
	int b;  //同じくもう一方(a < b)

	//前回の接触点と対応づけるための番号
	int idA;  //aの通し番号
	int idB;  //bの通し番号
	int id;  //同じ組の何番目の点か

	VEC2 point;  //接触点
	VEC2 normal;  //aからbへ向く法線
	float bias;  //反発で残す離れる速度

	float accumulatedNormal;  //これまでに与えた法線方向のインパルスの合計
	float accumulatedTangent;  //同じく接線方向(摩擦)

	S_CONTACT() {
		a = b = idA = idB = id = 0;
		bias = 0.0f;
		accumulatedNormal = 0.0f;
		accumulatedTangent = 0.0f;
	}
};

//接触点の数の上限
const int maxContactCnt = 8192;

//重なっているかもしれない物体の組
struct S_PAIR {
	int a;  //物体の番号
	int b;  //同じくもう一方(a < b)
};

//組の数の上限
const int maxPairCnt = 8192;

//物体(壁を含む)と、その数
BODY bodies[maxBodyCnt];
int bodyCnt = 0;

//次に追加する物体に付ける通し番号
static int nextBodyID = 0;

//今回の接触点
static S_CONTACT contacts[maxContactCnt];
static int contactCnt = 0;

//前回の接触点(ウォームスタート用)
static S_CONTACT prevContacts[maxContactCnt];
static int prevContactCnt = 0;

//物体の番号を、バウンディングボックスの上の端が高い順に並べたもの(組の絞り込み用)
static int sortedBodies[maxBodyCnt];
static int sortedBodyCnt = 0;

//重なっているかもしれない組
static S_PAIR pairs[maxPairCnt];
static int pairCnt = 0;

//物体を全部消す
void ClearBodies() {
	bodyCnt = 0;
	nextBodyID = 0;
	sortedBodyCnt = 0;

	contactCnt = 0;
	prevContactCnt = 0;
}

//空の物体を追加する
//呼んだあとにAddPartで部品を足し、最後にCalculateMassを呼ぶ
//戻り値: 追加した物体
//追加できなければNULL
BODY* AddBody(VEC2 center, float angle, UINT rgb) {
	if(bodyCnt >= maxBodyCnt) return NULL;

	BODY* body = &bodies[bodyCnt];
	body->Initialize(center, angle, rgb);
	body->SetID(nextBodyID);

	bodyCnt++;
	nextBodyID++;

	return body;
}

//動かない壁(長方形)を追加する
BODY* AddWall(VEC2 center, float w, float h, UINT rgb) {
	VEC2 points[4] = {
		VEC2(-w / 2.0f, -h / 2.0f),  //左上
		VEC2(w / 2.0f, -h / 2.0f),  //右上
		VEC2(w / 2.0f, h / 2.0f),  //右下
		VEC2(-w / 2.0f, h / 2.0f)  //左下
	};

	BODY* body = AddBody(center, 0.0f, rgb);
	if(body == NULL) return NULL;

	body->AddPart(points, 4);
	body->CalculateMass();
	body->SetStatic();

	return body;
}

//物体を消す
//後ろの物体を1つずつ前に詰める(並ぶ順と通し番号は変わらない)
void RemoveBody(int idx) {
	for(int i=idx; i<bodyCnt - 1; i++) bodies[i] = bodies[i + 1];

	bodyCnt--;
}

//通し番号がidの物体が、今何番目にあるか
//戻り値: 物体の番号
//無ければ-1
int FindBodyIdx(int id) {
	for(int i=0; i<bodyCnt; i++) {
		if(bodies[i].GetID() == id) return i;
	}

	return -1;
}

//組の絞り込み

//組を、物体の番号の小さい順に並べるための比較
static bool IsPairBefore(S_PAIR x, S_PAIR y) {
	if(x.a != y.a) return x.a < y.a;

	return x.b < y.b;
}

//バウンディングボックスが重なっている組を探す(平面走査)
//全部の組み合わせを調べる代わりに、物体を上の端の高さの順に並べ、縦の範囲が重なる相手だけを調べる
static void FindPairs() {
	//物体の数が変わったら、並びを作り直す
	if(sortedBodyCnt != bodyCnt) {
		for(int i=0; i<bodyCnt; i++) sortedBodies[i] = i;
		sortedBodyCnt = bodyCnt;
	}

	//上の端の高さの順に並べ替える(挿入ソート)
	//前回とほとんど同じ順なので、ずらす回数は少ない
	for(int i=1; i<sortedBodyCnt; i++) {
		int body = sortedBodies[i];
		float top = bodies[body].GetBoundingBoxLT().y;

		int j = i - 1;
		while(j >= 0 && bodies[sortedBodies[j]].GetBoundingBoxLT().y > top) {
			sortedBodies[j + 1] = sortedBodies[j];
			j--;
		}

		sortedBodies[j + 1] = body;
	}

	//上にある物体から順に、自分の下の端より上から始まる物体だけを相手にする
	pairCnt = 0;
	for(int i=0; i<sortedBodyCnt; i++) {
		int idxA = sortedBodies[i];
		float bottom = bodies[idxA].GetBoundingBoxRB().y;

		for(int j=i+1; j<sortedBodyCnt; j++) {
			int idxB = sortedBodies[j];
			if(bodies[idxB].GetBoundingBoxLT().y > bottom) break;  //これより後ろは、もっと下から始まるので重ならない

			if(bodies[idxA].IsStatic() && bodies[idxB].IsStatic()) continue;  //壁同士は何もしない
			if(!bodies[idxA].IsBoundingBoxOverlayed(&bodies[idxB])) continue;
			if(pairCnt >= maxPairCnt) continue;

			pairs[pairCnt].a = min(idxA, idxB);
			pairs[pairCnt].b = max(idxA, idxB);
			pairCnt++;
		}
	}

	//物体の番号の順に並べておく(接触点がこの順に作られるので、前回の接触点と同じ並びになる)
	std::sort(pairs, pairs + pairCnt, IsPairBefore);
}

//分離軸判定と押し出し

//頂点を軸に投影したときの範囲(影の両端)
static void Project(const VEC2* points, int pointCnt, VEC2 axis, float* minV, float* maxV) {
	*minV = Dot(points[0], axis);
	*maxV = *minV;

	for(int i=1; i<pointCnt; i++) {
		float d = Dot(points[i], axis);
		*minV = min(*minV, d);
		*maxV = max(*maxV, d);
	}
}

//頂点の平均(だいたいの中心)
static VEC2 GetAverage(const VEC2* points, int pointCnt) {
	VEC2 sum;
	for(int i=0; i<pointCnt; i++) sum += points[i];

	return sum / (float)pointCnt;
}

//2つの凸多角形が重なっているか(分離軸判定)
//各辺に垂直な軸に両方の頂点を投影し、影が重ならない軸が1つでもあれば重なっていない
//normal: 重なっていれば、押し出す向き(aからbへ向く)が入る
//影の重なりが一番小さい軸を選ぶ
//depth: 重なっていれば、重なりの深さが入る
//戻り値: 重なっていればtrue
static bool IsPolygonsOverlapped(const VEC2* pointsA, int cntA, const VEC2* pointsB, int cntB, VEC2* normal, float* depth) {
	*depth = FLT_MAX;

	//aの辺、bの辺の順に、辺に垂直な軸を取る
	for(int side=0; side<2; side++) {
		const VEC2* points = (side == 0) ? pointsA : pointsB;
		int cnt = (side == 0) ? cntA : cntB;

		for(int i=0; i<cnt; i++) {
			VEC2 axis = (points[(i + 1) % cnt] - points[i]).Perpendicular().Normalized();

			float minA, maxA, minB, maxB;
			Project(pointsA, cntA, axis, &minA, &maxA);
			Project(pointsB, cntB, axis, &minB, &maxB);

			//軸上での影の重なり幅
			//隙間のある軸が1つでもあれば重なっていない
			float overlap = min(maxA, maxB) - max(minA, minB);
			if(overlap <= 0.0f) return false;

			if(overlap < *depth) {
				*depth = overlap;
				*normal = axis;
			}
		}
	}

	//aからbへ向く向きにそろえる
	if(Dot(GetAverage(pointsB, cntB) - GetAverage(pointsA, cntA), *normal) < 0.0f) *normal = -(*normal);

	return true;
}

//接触点とインパルス(力積)

//接触点pointでの、aから見たbのdir方向の相対速度(法線方向なら、負のとき近づいている)
static float GetRelativeSpeed(BODY* a, BODY* b, VEC2 point, VEC2 dir) {
	return Dot(b->GetVelocityAt(point) - a->GetVelocityAt(point), dir);
}

//インパルスを速度に直すときの分母
//質量と慣性モーメントの分
//壁は質量の逆数が0なので動かない
static float GetMassTerm(BODY* a, BODY* b, VEC2 point, VEC2 dir) {
	float ra = Cross(point - a->GetCenter(), dir);
	float rb = Cross(point - b->GetCenter(), dir);

	return a->GetInvMass() + b->GetInvMass() + ra * ra * a->GetInvInertia() + rb * rb * b->GetInvInertia();
}

//反発で残す離れる速度(速くぶつかったときだけ跳ね返す)
static float GetBounceBias(BODY* a, BODY* b, VEC2 point, VEC2 normal) {
	float speed = GetRelativeSpeed(a, b, point, normal);
	if(speed > -bounceSpeed) return 0.0f;

	return -restitution * speed;
}

//接触点を集める(1点か2点)
//normal方向に最も進んだaの頂点と、最も手前にあるbの頂点を接線方向に並べ、
//両方の範囲が重なる部分の両端を接触点にする
static void AddContacts(int idxA, int partA, int idxB, int partB, const VEC2* pointsA, const VEC2* pointsB, VEC2 normal) {
	if(contactCnt + 2 > maxContactCnt) return;

	BODY* a = &bodies[idxA];
	BODY* b = &bodies[idxB];
	int cntA = a->GetPointCnt(partA);
	int cntB = b->GetPointCnt(partB);
	VEC2 tangent = normal.Perpendicular();

	//aの最も進んだ位置と、bの最も手前の位置
	float topA, bottomB, unused;
	Project(pointsA, cntA, normal, &unused, &topA);
	Project(pointsB, cntB, normal, &bottomB, &unused);

	//接触面にある頂点の、接線方向の範囲
	float minA = FLT_MAX;
	float maxA = -FLT_MAX;
	for(int i=0; i<cntA; i++) {
		if(Dot(pointsA[i], normal) < topA - contactEpsilon) continue;

		float t = Dot(pointsA[i], tangent);
		minA = min(minA, t);
		maxA = max(maxA, t);
	}

	float minB = FLT_MAX;
	float maxB = -FLT_MAX;
	for(int i=0; i<cntB; i++) {
		if(Dot(pointsB[i], normal) > bottomB + contactEpsilon) continue;

		float t = Dot(pointsB[i], tangent);
		minB = min(minB, t);
		maxB = max(maxB, t);
	}

	//両方の範囲が重なる部分
	//重ならないときは真ん中の1点にする
	float low = max(minA, minB);
	float high = min(maxA, maxB);
	if(low > high) low = high = (low + high) / 2.0f;

	float plane = (topA + bottomB) / 2.0f;  //接触面の位置

	S_CONTACT* contact = &contacts[contactCnt];
	contactCnt++;

	contact->a = idxA;
	contact->b = idxB;
	contact->idA = a->GetID();
	contact->idB = b->GetID();
	contact->id = (partA * maxPartCnt + partB) * 2;  //部品の組み合わせと何番目の点かで番号をつける
	contact->normal = normal;
	contact->point = normal * plane + tangent * low;
	contact->bias = GetBounceBias(a, b, contact->point, normal);
	contact->accumulatedNormal = 0.0f;
	contact->accumulatedTangent = 0.0f;

	//幅がなければ1点だけ
	if(high - low < contactEpsilon) return;

	S_CONTACT* contact2 = &contacts[contactCnt];
	contactCnt++;

	*contact2 = *contact;
	contact2->id = contact->id + 1;
	contact2->point = normal * plane + tangent * high;
	contact2->bias = GetBounceBias(a, b, contact2->point, normal);
}

//物体aの部品partAと物体bの部品partBの重なりを押し出して解消し、接触点を集める
static void PushApartParts(int idxA, int partA, int idxB, int partB) {
	BODY* a = &bodies[idxA];
	BODY* b = &bodies[idxB];

	VEC2 pointsA[maxPointCnt];
	VEC2 pointsB[maxPointCnt];
	a->GetWorldPoints(partA, pointsA);
	b->GetWorldPoints(partB, pointsB);

	VEC2 normal;
	float depth;
	if(!IsPolygonsOverlapped(pointsA, a->GetPointCnt(partA), pointsB, b->GetPointCnt(partB), &normal, &depth)) return;

	//重なりがいちばん浅い向きへ押し出す(軽い方が多く動く)
	//slopのぶんは残しておき、次のコマでも接触点が取れるようにする
	float weightA = a->GetInvMass();
	float weightB = b->GetInvMass();
	float push = max(depth - slop, 0.0f);

	a->MoveCenterPos(normal * (-push * weightA / (weightA + weightB)));
	b->MoveCenterPos(normal * (push * weightB / (weightA + weightB)));

	//押し出した後の位置で、接触点を集める
	a->GetWorldPoints(partA, pointsA);
	b->GetWorldPoints(partB, pointsB);
	AddContacts(idxA, partA, idxB, partB, pointsA, pointsB, normal);
}

//2つの物体の重なりを、部品の組み合わせごとに押し出す
static void PushApart(int idxA, int idxB) {
	int partCntA = bodies[idxA].GetPartCnt();
	int partCntB = bodies[idxB].GetPartCnt();

	for(int partA=0; partA<partCntA; partA++) {
		for(int partB=0; partB<partCntB; partB++) {
			PushApartParts(idxA, partA, idxB, partB);
		}
	}
}

//接触点の並び順の比較(aの通し番号、bの通し番号、点の番号の順に比べる)
//戻り値: xがyより前なら負、同じ接触点なら0、後なら正
static int CompareContact(S_CONTACT* x, S_CONTACT* y) {
	if(x->idA != y->idA) return x->idA - y->idA;
	if(x->idB != y->idB) return x->idB - y->idB;

	return x->id - y->id;
}

//前回と同じ接触点があれば、前回のインパルスを引き継いで先に与えておく(ウォームスタート)
//毎回0から積み上げ直すより、少ない周回で積み重ねが落ち着く
//今回と前回の接触点はどちらも同じ順に並んでいるので、前回の側の位置を今回に追いつくまで進めるだけで対応が取れる(尺取り法)
static void WarmStart() {
	int j = 0;  //前回の接触点の、今見ている位置

	for(int i=0; i<contactCnt; i++) {
		S_CONTACT* contact = &contacts[i];

		while(j < prevContactCnt && CompareContact(&prevContacts[j], contact) < 0) j++;
		if(j >= prevContactCnt) break;

		S_CONTACT* prev = &prevContacts[j];
		if(CompareContact(prev, contact) != 0) continue;  //前回にはなかった接触点

		contact->accumulatedNormal = prev->accumulatedNormal;
		contact->accumulatedTangent = prev->accumulatedTangent;

		VEC2 impulse = contact->normal * contact->accumulatedNormal + contact->normal.Perpendicular() * contact->accumulatedTangent;
		bodies[contact->a].ApplyImpulse(contact->point, -impulse);
		bodies[contact->b].ApplyImpulse(contact->point, impulse);
	}
}

//接触点で、近づく速度を打ち消すインパルスを与える(摩擦も含む)
//何周も呼ばれるので、これまでの合計を見て「引っ張る向き」にならないようにする(合計を0以上に制限)
static void SolveContact(S_CONTACT* contact) {
	BODY* a = &bodies[contact->a];
	BODY* b = &bodies[contact->b];
	VEC2 point = contact->point;
	VEC2 normal = contact->normal;

	//法線方向(垂直抗力にあたる)
	float normalSpeed = GetRelativeSpeed(a, b, point, normal);
	float massTerm = GetMassTerm(a, b, point, normal);
	if(massTerm == 0.0f) return;

	float impulse = (contact->bias - normalSpeed) / massTerm;
	if(contact->accumulatedNormal + impulse < 0.0f) impulse = -contact->accumulatedNormal;  //合計が0を下回る分は与えない
	contact->accumulatedNormal += impulse;

	a->ApplyImpulse(point, normal * -impulse);
	b->ApplyImpulse(point, normal * impulse);

	//接線方向(摩擦)
	//滑る速度を打ち消すが、合計は「摩擦係数 × 垂直抗力の合計」までに抑える
	VEC2 tangent = normal.Perpendicular();
	float tangentSpeed = GetRelativeSpeed(a, b, point, tangent);
	float frictionImpulse = -tangentSpeed / GetMassTerm(a, b, point, tangent);

	float maxFriction = friction * contact->accumulatedNormal;
	if(contact->accumulatedTangent + frictionImpulse > maxFriction) frictionImpulse = maxFriction - contact->accumulatedTangent;
	if(contact->accumulatedTangent + frictionImpulse < -maxFriction) frictionImpulse = -maxFriction - contact->accumulatedTangent;
	contact->accumulatedTangent += frictionImpulse;

	a->ApplyImpulse(point, tangent * -frictionImpulse);
	b->ApplyImpulse(point, tangent * frictionImpulse);
}

//1フレーム分進める
//dt: 1フレームの時間(秒)
void StepPhysics(float dt) {
	float subDt = dt / subStepCnt;

	for(int step=0; step<subStepCnt; step++) {
		//重力で動かす
		for(int i=0; i<bodyCnt; i++) bodies[i].UpdatePhysics(subDt);

		//重なりを押し出して、接触点を集める(何周かして落ち着かせる)
		for(int iter=0; iter<positionIterations; iter++) {
			contactCnt = 0;
			for(int i=0; i<bodyCnt; i++) bodies[i].UpdateBoundingBox();

			FindPairs();
			for(int i=0; i<pairCnt; i++) PushApart(pairs[i].a, pairs[i].b);
		}

		//接触点でインパルスを与える(何周もして全体をなじませる)
		WarmStart();
		for(int iter=0; iter<velocityIterations; iter++) {
			for(int i=0; i<contactCnt; i++) SolveContact(&contacts[i]);
		}

		//次回のウォームスタート用に覚えておく
		for(int i=0; i<contactCnt; i++) prevContacts[i] = contacts[i];
		prevContactCnt = contactCnt;
	}

	//静止中の判定用に、止まっているフレーム数を数える
	for(int i=0; i<bodyCnt; i++) {
		if(bodies[i].IsStatic()) continue;

		bodies[i].UpdateRestCnt();
	}
}

//このコマの接触点の数
int GetContactCnt() { return contactCnt; }

//接触点contactでぶつかっている2つの物体の番号
void GetContactPair(int contact, int* a, int* b) {
	*a = contacts[contact].a;
	*b = contacts[contact].b;
}
