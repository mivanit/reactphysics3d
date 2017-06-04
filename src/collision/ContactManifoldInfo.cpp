/********************************************************************************
* ReactPhysics3D physics library, http://www.reactphysics3d.com                 *
* Copyright (c) 2010-2016 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "ContactManifoldInfo.h"

using namespace reactphysics3d;

// Constructor
ContactManifoldInfo::ContactManifoldInfo(Allocator& allocator)
     : mContactPointsList(nullptr), mAllocator(allocator), mNbContactPoints(0) {}

// Destructor
ContactManifoldInfo::~ContactManifoldInfo() {

    // Remove all the contact points
    reset();
}

// Add a new contact point into the manifold
void ContactManifoldInfo::addContactPoint(const Vector3& contactNormal, decimal penDepth,
                     const Vector3& localPt1, const Vector3& localPt2) {

    assert(penDepth > decimal(0.0));

    // Create the contact point info
    ContactPointInfo* contactPointInfo = new (mAllocator.allocate(sizeof(ContactPointInfo)))
            ContactPointInfo(contactNormal, penDepth, localPt1, localPt2);

    // Add it into the linked list of contact points
    contactPointInfo->next = mContactPointsList;
    mContactPointsList = contactPointInfo;

    mNbContactPoints++;
}

// Remove all the contact points
void ContactManifoldInfo::reset() {

    // Delete all the contact points in the linked list
    ContactPointInfo* element = mContactPointsList;
    while(element != nullptr) {
        ContactPointInfo* elementToDelete = element;
        element = element->next;

        // Delete the current element
        mAllocator.release(elementToDelete, sizeof(ContactPointInfo));
    }

    mContactPointsList = nullptr;
    mNbContactPoints = 0;
}

// Reduce the number of points in the contact manifold
// This is based on the technique described by Dirk Gregorius in his
// "Contacts Creation" GDC presentation
void ContactManifoldInfo::reduce(const Transform& shape1ToWorldTransform) {

    assert(mContactPointsList != nullptr);

    // TODO : Implement this (do not forget to deallocate removed points)

    // The following algorithm only works to reduce to 4 contact points
    assert(MAX_CONTACT_POINTS_IN_MANIFOLD == 4);

    // If there are too many contact points in the manifold
    if (mNbContactPoints > MAX_CONTACT_POINTS_IN_MANIFOLD) {

        ContactPointInfo* pointsToKeep[MAX_CONTACT_POINTS_IN_MANIFOLD];

        //  Compute the initial contact point we need to keep.
        // The first point we keep is always the point in a given
        // constant direction (in order to always have same contact points
        // between frames for better stability)

        const Transform worldToShape1Transform = shape1ToWorldTransform.getInverse();

        // Compute the contact normal of the manifold (we use the first contact point)
        // in the local-space of the first collision shape
        const Vector3 contactNormalShape1Space = worldToShape1Transform.getOrientation() * mContactPointsList->normal;

        // Compute a search direction
        const Vector3 searchDirection(1, 1, 1);
        ContactPointInfo* element = mContactPointsList;
        pointsToKeep[0] = element;
        decimal maxDotProduct = searchDirection.dot(element->localPoint1);
        element = element->next;
        while(element != nullptr) {

            decimal dotProduct = searchDirection.dot(element->localPoint1);
            if (dotProduct > maxDotProduct) {
                maxDotProduct = dotProduct;
                pointsToKeep[0] = element;
            }
            element = element->next;
        }

        // Compute the second contact point we need to keep.
        // The second point we keep is the one farthest away from the first point.

        decimal maxDistance = decimal(0.0);
        element = mContactPointsList;
        while(element != nullptr) {

            if (element == pointsToKeep[0]) {
                element = element->next;
                continue;
            }

            decimal distance = (pointsToKeep[0]->localPoint1 - element->localPoint1).lengthSquare();
            if (distance >= maxDistance) {
                maxDistance = distance;
                pointsToKeep[1] = element;
            }
            element = element->next;
        }
        assert(pointsToKeep[1] != nullptr);

        // Compute the third contact point we need to keep.
        // The second point is the one producing the triangle with the larger area
        // with first and second point.

        // We compute the most positive or most negative triangle area (depending on winding)
        ContactPointInfo* thirdPointMaxArea = nullptr;
        ContactPointInfo* thirdPointMinArea = nullptr;
        decimal minArea = decimal(0.0);
        decimal maxArea = decimal(0.0);
        bool isPreviousAreaPositive = true;
        element = mContactPointsList;
        while(element != nullptr) {

            if (element == pointsToKeep[0] || element == pointsToKeep[1]) {
                element = element->next;
                continue;
            }

            const Vector3 newToFirst = pointsToKeep[0]->localPoint1 - element->localPoint1;
            const Vector3 newToSecond = pointsToKeep[1]->localPoint1 - element->localPoint1;

            // Compute the triangle area
            decimal area = newToFirst.cross(newToSecond).dot(contactNormalShape1Space);

            if (area >= maxArea) {
                maxArea = area;
                thirdPointMaxArea = element;
            }
            if (area <= minArea) {
                minArea = area;
                thirdPointMinArea = element;
            }
            element = element->next;
        }
        assert(minArea <= decimal(0.0));
        assert(maxArea >= decimal(0.0));
        if (maxArea > (-minArea)) {
            isPreviousAreaPositive = true;
            pointsToKeep[2] = thirdPointMaxArea;
        }
        else {
            isPreviousAreaPositive = false;
            pointsToKeep[2] = thirdPointMinArea;
        }
        assert(pointsToKeep[2] != nullptr);

        // Compute the 4th point by choosing the triangle that add the most
        // triangle area to the previous triangle and has opposite sign area (opposite winding)

        decimal largestArea = decimal(0.0); // Largest area (positive or negative)
        element = mContactPointsList;

        // For each remaining point
        while(element != nullptr) {

            if (element == pointsToKeep[0] || element == pointsToKeep[1] || element == pointsToKeep[2]) {
                element = element->next;
                continue;
            }

            // For each edge of the triangle made by the first three points
            for (uint i=0; i<3; i++) {

                uint edgeVertex1Index = i;
                uint edgeVertex2Index = i < 2 ? i + 1 : 0;

                const Vector3 newToFirst = pointsToKeep[edgeVertex1Index]->localPoint1 - element->localPoint1;
                const Vector3 newToSecond = pointsToKeep[edgeVertex2Index]->localPoint1 - element->localPoint1;

                // Compute the triangle area
                decimal area = newToFirst.cross(newToSecond).dot(contactNormalShape1Space);

                // We are looking at the triangle with maximal area (positive or negative).
                // If the previous area is positive, we are looking at negative area now.
                // If the previous area is negative, we are looking at the positive area now.
                if (isPreviousAreaPositive && area < largestArea) {
                    largestArea = area;
                    pointsToKeep[3] = element;
                }
                else if (!isPreviousAreaPositive && area > largestArea) {
                    largestArea = area;
                    pointsToKeep[3] = element;
                }

                element = element->next;
            }
        }
        assert(pointsToKeep[3] != nullptr);

        // Delete the contact points we do not want to keep from the linked list
        element = mContactPointsList;
        ContactPointInfo* previousElement = nullptr;
        while(element != nullptr) {

            // Skip the points we want to keep
            if (element == pointsToKeep[0] || element == pointsToKeep[1] ||
                element == pointsToKeep[2] || element == pointsToKeep[3]) {

                previousElement = element;
                element = element->next;
                continue;
            }

            ContactPointInfo* elementToDelete = element;
            if (previousElement != nullptr) {
                previousElement->next = elementToDelete->next;
            }
            else {
                mContactPointsList = elementToDelete->next;
            }
            element = element->next;

            // Delete the current element
            mAllocator.release(elementToDelete, sizeof(ContactPointInfo));
        }

        mNbContactPoints = 4;
    }
}

/// Return the largest penetration depth among the contact points
decimal ContactManifoldInfo::getLargestPenetrationDepth() const {

    decimal maxDepth = decimal(0.0);
    ContactPointInfo* element = mContactPointsList;
    while(element != nullptr) {

        if (element->penetrationDepth > maxDepth) {
            maxDepth = element->penetrationDepth;
        }

        element = element->next;
    }

    return maxDepth;
}

