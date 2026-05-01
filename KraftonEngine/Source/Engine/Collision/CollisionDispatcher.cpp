#include "CollisionDispatcher.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include <cmath>

// ---- Geometry helpers ----

static FVector ClosestPointOnSegment(const FVector& P0, const FVector& P1, const FVector& Q) {
	FVector seg  = P1 - P0;
	float   len2 = seg.Dot(seg);
	if (len2 < 1e-10f) return P0;
	float t = (Q - P0).Dot(seg) / len2;
	t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
	return P0 + seg * t;
}

static FVector ClosestPointOnOBB(
	const FVector& Center, const FVector& Ax0, const FVector& Ax1, const FVector& Ax2,
	const FVector& Ext, const FVector& P)
{
	FVector d  = P - Center;
	float   c0 = d.Dot(Ax0); c0 = c0 < -Ext.X ? -Ext.X : (c0 > Ext.X ? Ext.X : c0);
	float   c1 = d.Dot(Ax1); c1 = c1 < -Ext.Y ? -Ext.Y : (c1 > Ext.Y ? Ext.Y : c1);
	float   c2 = d.Dot(Ax2); c2 = c2 < -Ext.Z ? -Ext.Z : (c2 > Ext.Z ? Ext.Z : c2);
	return Center + Ax0 * c0 + Ax1 * c1 + Ax2 * c2;
}

static void GetCapsuleSegment(const UCapsuleComponent* C, FVector& OutP0, FVector& OutP1) {
	FVector Center = C->GetWorldLocation();
	FVector Up     = C->GetUpVector().Normalized();
	float   SegHH  = C->GetCapsuleHalfHeight() - C->GetCapsuleRadius();
	if (SegHH < 0.f) SegHH = 0.f;
	OutP0 = Center - Up * SegHH;
	OutP1 = Center + Up * SegHH;
}

// Segment-to-segment squared distance (Ericson, Real-Time Collision Detection)
static float SegmentSegmentDistSq(const FVector& P0, const FVector& P1, const FVector& Q0, const FVector& Q1) {
	FVector d1 = P1 - P0;
	FVector d2 = Q1 - Q0;
	FVector r  = P0 - Q0;
	float   a  = d1.Dot(d1);
	float   e  = d2.Dot(d2);
	float   f  = d2.Dot(r);
	float   s, t;

	if (a < 1e-10f && e < 1e-10f) {
		return r.Dot(r);
	}
	if (a < 1e-10f) {
		s = 0.f;
		t = f / e;
		t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
	} else {
		float c = d1.Dot(r);
		if (e < 1e-10f) {
			t = 0.f;
			s = -c / a;
			s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
		} else {
			float b     = d1.Dot(d2);
			float denom = a * e - b * b;
			if (fabsf(denom) > 1e-10f) {
				s = (b * f - c * e) / denom;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			} else {
				s = 0.f;
			}
			t = (b * s + f) / e;
			if (t < 0.f) {
				t = 0.f;
				s = -c / a;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			} else if (t > 1.f) {
				t = 1.f;
				s = (b - c) / a;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			}
		}
	}
	FVector diff = (P0 + d1 * s) - (Q0 + d2 * t);
	return diff.Dot(diff);
}

// ---- SAT helper for OBB-OBB ----

static bool SATNoSeparation(
	const FVector& L, const FVector& d,
	const FVector& a0, const FVector& a1, const FVector& a2, const FVector& eA,
	const FVector& b0, const FVector& b1, const FVector& b2, const FVector& eB)
{
	float ra = fabsf(L.Dot(a0)) * eA.X + fabsf(L.Dot(a1)) * eA.Y + fabsf(L.Dot(a2)) * eA.Z;
	float rb = fabsf(L.Dot(b0)) * eB.X + fabsf(L.Dot(b1)) * eB.Y + fabsf(L.Dot(b2)) * eB.Z;
	return fabsf(L.Dot(d)) <= ra + rb;
}

// ---- Collision functions ----

static bool BoxBoxCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent* BoxA = static_cast<const UBoxComponent*>(A);
	const UBoxComponent* BoxB = static_cast<const UBoxComponent*>(B);

	FVector CA = BoxA->GetWorldLocation();
	FVector CB = BoxB->GetWorldLocation();
	FVector d  = CB - CA;

	FVector a0 = BoxA->GetForwardVector().Normalized();
	FVector a1 = BoxA->GetRightVector().Normalized();
	FVector a2 = BoxA->GetUpVector().Normalized();
	FVector eA = BoxA->GetBoxExtent();

	FVector b0 = BoxB->GetForwardVector().Normalized();
	FVector b1 = BoxB->GetRightVector().Normalized();
	FVector b2 = BoxB->GetUpVector().Normalized();
	FVector eB = BoxB->GetBoxExtent();

	// 15 SAT axes: 3 face normals each + 9 edge cross products
	FVector axes[15] = {
		a0, a1, a2, b0, b1, b2,
		a0.Cross(b0), a0.Cross(b1), a0.Cross(b2),
		a1.Cross(b0), a1.Cross(b1), a1.Cross(b2),
		a2.Cross(b0), a2.Cross(b1), a2.Cross(b2),
	};

	for (const FVector& axis : axes) {
		float lenSq = axis.Dot(axis);
		if (lenSq < 1e-10f) continue;	// degenerate cross product (parallel edges)
		FVector L = axis * (1.f / sqrtf(lenSq));
		if (!SATNoSeparation(L, d, a0, a1, a2, eA, b0, b1, b2, eB))
			return false;
	}
	return true;
}

static bool SphereSphereCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const USphereComponent* SA = static_cast<const USphereComponent*>(A);
	const USphereComponent* SB = static_cast<const USphereComponent*>(B);
	float rSum = SA->GetSphereRadius() + SB->GetSphereRadius();
	return FVector::DistSquared(SA->GetWorldLocation(), SB->GetWorldLocation()) <= rSum * rSum;
}

static bool BoxSphereCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent*    Box;
	const USphereComponent* Sphere;
	if (dynamic_cast<const UBoxComponent*>(A)) {
		Box    = static_cast<const UBoxComponent*>(A);
		Sphere = static_cast<const USphereComponent*>(B);
	} else {
		Box    = static_cast<const UBoxComponent*>(B);
		Sphere = static_cast<const USphereComponent*>(A);
	}

	FVector Ax0 = Box->GetForwardVector().Normalized();
	FVector Ax1 = Box->GetRightVector().Normalized();
	FVector Ax2 = Box->GetUpVector().Normalized();
	FVector Ext = Box->GetBoxExtent();
	FVector SC  = Sphere->GetWorldLocation();
	float   SR  = Sphere->GetSphereRadius();

	FVector Closest = ClosestPointOnOBB(Box->GetWorldLocation(), Ax0, Ax1, Ax2, Ext, SC);
	return FVector::DistSquared(Closest, SC) <= SR * SR;
}

static bool BoxCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent*     Box;
	const UCapsuleComponent* Capsule;
	if (dynamic_cast<const UBoxComponent*>(A)) {
		Box     = static_cast<const UBoxComponent*>(A);
		Capsule = static_cast<const UCapsuleComponent*>(B);
	} else {
		Box     = static_cast<const UBoxComponent*>(B);
		Capsule = static_cast<const UCapsuleComponent*>(A);
	}

	FVector Ax0 = Box->GetForwardVector().Normalized();
	FVector Ax1 = Box->GetRightVector().Normalized();
	FVector Ax2 = Box->GetUpVector().Normalized();
	FVector Ext = Box->GetBoxExtent();
	FVector BC  = Box->GetWorldLocation();
	float   CR  = Capsule->GetCapsuleRadius();

	FVector P0, P1;
	GetCapsuleSegment(Capsule, P0, P1);

	// Test capsule endpoints and the segment point closest to the box center
	FVector candidates[3] = { P0, P1, ClosestPointOnSegment(P0, P1, BC) };
	for (const FVector& P : candidates) {
		FVector Closest = ClosestPointOnOBB(BC, Ax0, Ax1, Ax2, Ext, P);
		if (FVector::DistSquared(P, Closest) <= CR * CR)
			return true;
	}
	return false;
}

static bool SphereCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const USphereComponent*  Sphere;
	const UCapsuleComponent* Capsule;
	if (dynamic_cast<const USphereComponent*>(A)) {
		Sphere  = static_cast<const USphereComponent*>(A);
		Capsule = static_cast<const UCapsuleComponent*>(B);
	} else {
		Sphere  = static_cast<const USphereComponent*>(B);
		Capsule = static_cast<const UCapsuleComponent*>(A);
	}

	FVector P0, P1;
	GetCapsuleSegment(Capsule, P0, P1);
	FVector SC      = Sphere->GetWorldLocation();
	FVector Closest = ClosestPointOnSegment(P0, P1, SC);
	float   rSum    = Sphere->GetSphereRadius() + Capsule->GetCapsuleRadius();
	return FVector::DistSquared(Closest, SC) <= rSum * rSum;
}

static bool CapsuleCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UCapsuleComponent* CA = static_cast<const UCapsuleComponent*>(A);
	const UCapsuleComponent* CB = static_cast<const UCapsuleComponent*>(B);

	FVector P0, P1, Q0, Q1;
	GetCapsuleSegment(CA, P0, P1);
	GetCapsuleSegment(CB, Q0, Q1);

	float rSum   = CA->GetCapsuleRadius() + CB->GetCapsuleRadius();
	float distSq = SegmentSegmentDistSq(P0, P1, Q0, Q1);
	return distSq <= rSum * rSum;
}

void FCollisionDispatcher::Init() {
	Register("UBoxComponent",     "UBoxComponent",     BoxBoxCollision);
	Register("UBoxComponent",     "USphereComponent",  BoxSphereCollision);
	Register("UBoxComponent",     "UCapsuleComponent", BoxCapsuleCollision);
	Register("USphereComponent",  "USphereComponent",  SphereSphereCollision);
	Register("USphereComponent",  "UCapsuleComponent", SphereCapsuleCollision);
	Register("UCapsuleComponent", "UCapsuleComponent", CapsuleCapsuleCollision);
}
