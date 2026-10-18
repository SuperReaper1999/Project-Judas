#pragma once

#include "Contacts.h"
#include "ContactGeometryInternal.h"

// Derived data for exactly one represented parent orientation. No heap
// allocation or exact expansion work is performed by this preparation.
struct ContactPreparedOrientation {
    glm::quat source;
    contact_geometry::Rotation<contact_geometry::Iv> interval;
    contact_geometry::Rotation<double> number;
    glm::dmat3 rotation;
    contact_geometry::M<contact_geometry::Iv> normalizedInterval;

    explicit ContactPreparedOrientation(const glm::quat& orientation);
    bool Matches(const glm::quat& orientation) const;
};

// Keep the individual products: summing these before adding position would
// reassociate the established interval bound calculation.
struct PreparedBoxBound {
    std::array<std::array<contact_geometry::Iv, 3>, 3> offsetTerms;
    std::array<contact_geometry::Iv, 3> extent;
};

struct PreparedShapeBounds {
    ShapeType type = ShapeType::Sphere;
    double radialRadius = 0.0;
    PreparedBoxBound box;
    std::vector<PreparedBoxBound> children;
};
