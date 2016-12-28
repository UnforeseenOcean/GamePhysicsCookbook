#include "Rigidbody.h"
#include "Compare.h"
#include "FixedFunctionPrimitives.h"

void Rigidbody::ApplyForces() {
	forces = vec3();// GRAVITY_CONST;
}

void  Rigidbody::AddRotationalImpulse(const vec3& point, const vec3& impulse) {
	vec3 centerOfMass = position;
	vec3 torque = Cross(point - centerOfMass, impulse);

	/*vec3 inertia = TensorVector();
	vec3 angAccel(
		inertia.x == 0.0f ? 0.0f : torque.x / inertia.x,
		inertia.y == 0.0f ? 0.0f : torque.y / inertia.y,
		inertia.z == 0.0f ? 0.0f : torque.z / inertia.z
	);*/

	vec3 angAccel = torque * InvTensor();

	angVel = angVel + angAccel;
}

void Rigidbody::AddLinearImpulse(const vec3& impulse) {
	velocity = velocity + impulse;
}

float Rigidbody::InvMass() {
	if (mass == 0.0f) {
		return 0.0f;
	}
	return 1.0f / mass;
}

void Rigidbody::SynchCollisionVolumes() {
	sphere.position = position;
	box.position = position;
	box.orientation = Rotation3x3(orientation.x, orientation.y, orientation.z);
}

void Rigidbody::Render() {
	SynchCollisionVolumes();

	if (type == RIGIDBODY_TYPE_SPHERE) {
		::Render(sphere);
	}
	else if (type == RIGIDBODY_TYPE_BOX) {
		::Render(box);
	}
}



void Rigidbody::Update(float dt) {
	// Linear
	vec3 acceleration = forces * InvMass(); // A = F / M
	velocity = velocity + acceleration * dt;
	position = position + velocity * dt;

	if (type == RIGIDBODY_TYPE_BOX) {
		// Angular
		/*vec3 inertia = TensorVector();
		vec3 angAccel(
			inertia.x == 0.0f ? 0.0f : torques.x / inertia.x,
			inertia.y == 0.0f ? 0.0f : torques.y / inertia.y,
			inertia.z == 0.0f ? 0.0f : torques.z / inertia.z
		);*/
		vec3 angAccel = torques * InvTensor();
		angVel = angVel + angAccel * dt;
		orientation = orientation + angVel * dt;
	}

	SynchCollisionVolumes();
}

CollisionManifold FindCollisionFeatures(Rigidbody& ra, Rigidbody& rb) {
	CollisionManifold result;
	ResetCollisionManifold(&result);

	if (ra.type == RIGIDBODY_TYPE_SPHERE) {
		if (rb.type == RIGIDBODY_TYPE_SPHERE) {
			result = FindCollisionFeatures(ra.sphere, rb.sphere);
		}
		else if (rb.type == RIGIDBODY_TYPE_BOX) {
			result = FindCollisionFeatures(rb.box, ra.sphere);
			result.normal = result.normal * -1.0f;
		}
	}
	else if (ra.type == RIGIDBODY_TYPE_BOX) {
		if (rb.type == RIGIDBODY_TYPE_BOX) {
			result = FindCollisionFeatures(ra.box, rb.box);
		}
		else if (rb.type == RIGIDBODY_TYPE_SPHERE) {
			result = FindCollisionFeatures(ra.box, rb.sphere);
		}
	}


	return result;
}

void ApplyImpulse(Rigidbody& A, Rigidbody& B, const CollisionManifold& M) {
	// Linear impulse
	float invMass1 = A.InvMass();
	float invMass2 = B.InvMass();
	float invMassSum = invMass1 + invMass2;

	if (invMassSum == 0.0f) {
		return; // Both objects have infinate mass!
	}

	// Relative velocity
	vec3 relativeVel = B.velocity - A.velocity;
	// Relative collision normal
	vec3 relativeNorm = M.normal;

	// Moving away from each other? Do nothing!
	if (Dot(relativeVel, relativeNorm) > 0.0f) {
		return;
	}

	float e = fminf(A.cor, B.cor);
	float numerator = (-(1.0f + e) * Dot(relativeVel, relativeNorm));
	float j = numerator / invMassSum;

	vec3 impulse = relativeNorm * j;
	A.velocity = A.velocity - impulse *  invMass1;
	B.velocity = B.velocity + impulse *  invMass2;

	// Friction
	float sf = sqrtf(A.staticFriction * A.staticFriction + B.staticFriction * B.staticFriction);
	float df = sqrtf(A.dynamicFriction * A.dynamicFriction + B.dynamicFriction * B.dynamicFriction);

	relativeVel = B.velocity - A.velocity;
	vec3 t = relativeVel - relativeNorm * Dot(relativeVel, relativeNorm);
	if (CMP(MagnitudeSq(t), 0.0f)) {
		return;
	}
	Normalize(t);

	float jt = -Dot(relativeVel, t);
	jt /= (invMass1 + invMass2);
	if (M.contacts.size() > 0.0f) {
		jt /= (float)M.contacts.size();
	}

	if (CMP(jt, 0.0f)) {
		return;
	}

	vec3 tangentImpuse;
	if (fabsf(jt) < j * sf) {
		tangentImpuse = t * jt;
	}
	else {
		tangentImpuse = t * -j * df;
	}

	A.velocity = A.velocity - tangentImpuse *  invMass1;
	B.velocity = B.velocity + tangentImpuse *  invMass2;
}