// Stage 0 numerical experiment only: not an engine component or a second world.
// Compare reduced mobility with exact bilateral velocity rows. Neither prototype
// supplies collision discovery, MK2 ordering, persistence, or a character motor.
#include "CameraProjection.h"
#include "orthographic_stage0.h"

#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using V = glm::dvec3;
using M = glm::dmat3;
int checks = 0;
void Require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool Near(V a, V b, double tolerance = 2e-12) {
    return glm::length(a - b) <= tolerance;
}
struct Frame {
    V x, y, n;
    explicit Frame(glm::dquat orientation)
        : x(orientation * V(1, 0, 0)), y(orientation * V(0, 1, 0)),
          n(orientation * V(0, 0, 1)) {}
};
struct Velocity { V linear{0}, angular{0}; };

struct Reduced {
    Frame f;
    double mass, inertia;
    glm::dvec2 linear{0};
    double angular = 0;
    Reduced(Frame frame, double m, double i) : f(frame), mass(m), inertia(i) {}
    void Impulse(V impulse, V arm) {
        linear += glm::dvec2(glm::dot(f.x, impulse), glm::dot(f.y, impulse)) / mass;
        angular += glm::dot(f.n, glm::cross(arm, impulse)) / inertia;
    }
    Velocity Get() const { return {f.x * linear.x + f.y * linear.y, f.n * angular}; }
    double Energy() const { return .5 * mass * glm::dot(linear, linear) + .5 * inertia * angular * angular; }
};

// An invertible embedded lamina inertia, with forbidden angular velocity rows
// x.w=0, y.w=0 and forbidden normal translation n.v=0. The reactions are solved
// at the velocity-response boundary, NOT a pose/velocity clamp after integration.
struct Rows {
    Frame f;
    double mass;
    M inverseInertia, inertia;
    Velocity velocity;
    V linearReaction{0}, angularReaction{0};
    Rows(Frame frame, double m, double iz) : f(frame), mass(m) {
        const M basis(f.x, f.y, f.n);
        // Different in-plane principal inertias exercise the 2-row block solve.
        const M principal(V(iz * .3, 0, 0), V(0, iz * .7, 0), V(0, 0, iz));
        inertia = basis * principal * glm::transpose(basis);
        inverseInertia = glm::inverse(inertia);
    }
    void Impulse(V impulse, V arm, bool solveRows = true) {
#ifdef JUDAS_PLANAR_PROBE_DISABLE_ROWS
        solveRows = false; // Deliberately broken build: the driver requires failure.
#endif
        velocity.linear += impulse / mass;
        velocity.angular += inverseInertia * glm::cross(arm, impulse);
        if (!solveRows) return;
        const V r = -mass * glm::dot(f.n, velocity.linear) * f.n;
        velocity.linear += r / mass;
        linearReaction += r;
        const double a = glm::dot(f.x, inverseInertia * f.x);
        const double b = glm::dot(f.x, inverseInertia * f.y);
        const double d = glm::dot(f.y, inverseInertia * f.y);
        const double u = -glm::dot(f.x, velocity.angular);
        const double v = -glm::dot(f.y, velocity.angular);
        const double determinant = a * d - b * b;
        const V torque = ((d * u - b * v) * f.x + (a * v - b * u) * f.y) / determinant;
        velocity.angular += inverseInertia * torque;
        angularReaction += torque;
    }
    Velocity Get() const { return velocity; }
    double Energy() const {
        return .5 * mass * glm::dot(velocity.linear, velocity.linear) +
               .5 * glm::dot(velocity.angular, inertia * velocity.angular);
    }
};

template<class Body> double EffectiveResponse(Body body, V arm, V normal) {
    const Velocity before = body.Get();
    body.Impulse(normal, arm);
    const Velocity after = body.Get();
    return glm::dot(normal, after.linear - before.linear + glm::cross(after.angular - before.angular, arm));
}
template<class Body> void Impact(Body& body, V arm, V normal, double restitution) {
    const auto velocity = body.Get();
    const double approach = glm::dot(normal, velocity.linear + glm::cross(velocity.angular, arm));
    if (approach < 0) body.Impulse(normal * (-(1 + restitution) * approach / EffectiveResponse(body, arm, normal)), arm);
}

void MobilityContracts(Frame f) {
    // Uniform rectangular lamina, full width 2 and height 4: Izz=m(w²+h²)/12.
    constexpr double mass = 3, inertia = mass * (4 + 16) / 12;
    Reduced reduced(f, mass, inertia);
    Rows rows(f, mass, inertia), broken(f, mass, inertia);
    const V impulse = 6.0 * f.x + 3.0 * f.y + 9.0 * f.n;
    const V arm = 2.0 * f.y;
    reduced.Impulse(impulse, arm);
    rows.Impulse(impulse, arm);
    broken.Impulse(impulse, arm, false);
    Require(Near(reduced.Get().linear, 2.0 * f.x + f.y), "analytic planar mass response");
    Require(Near(reduced.Get().angular, -12.0 / inertia * f.n), "analytic lamina angular response");
    Require(Near(rows.Get().linear, reduced.Get().linear), "rows/reduced linear equivalence");
    Require(Near(rows.Get().angular, reduced.Get().angular), "rows/reduced angular equivalence");
    Require(std::abs(rows.Energy() - reduced.Energy()) < 2e-12, "valid nonsingular/reduced energy");
    Require(Near(rows.linearReaction, -9.0 * f.n), "normal reaction balances rejected impulse");
    Require(Near(rows.angularReaction, -18.0 * f.x), "angular reaction balances forbidden torque");
    Require(std::abs(glm::dot(broken.Get().linear, f.n)) > 1, "broken unconstrained response detected");
    Require(glm::length(glm::cross(broken.Get().angular, f.n)) > 1, "broken angular response detected");

    for (int i = 0; i < 1000; ++i) {
        const V p = .01 * (std::sin(i * .17) * f.x + std::cos(i * .31) * f.y + f.n);
        const V r = .3 * f.x - .2 * f.y;
        reduced.Impulse(p, r);
        rows.Impulse(p, r);
    }
    Require(Near(rows.Get().linear, reduced.Get().linear, 1e-10), "repeated linear response");
    Require(Near(rows.Get().angular, reduced.Get().angular, 1e-10), "repeated angular response");
    Require(std::abs(glm::dot(rows.Get().linear, f.n)) < 1e-12, "forbidden translation leakage");
    Require(glm::length(glm::cross(rows.Get().angular, f.n)) < 1e-12, "forbidden angular leakage");

    for (double restitution : {0.0, .5, 1.0}) {
        Reduced a(f, mass, inertia);
        Rows b(f, mass, inertia);
        a.Impulse(-6.0 * f.y, V(0));
        b.Impulse(-6.0 * f.y, V(0));
        const V contactArm = .4 * f.x - .7 * f.y;
        const double before = a.Energy();
        Impact(a, contactArm, f.y, restitution);
        Impact(b, contactArm, f.y, restitution);
        Require(Near(a.Get().linear, b.Get().linear), "off-centre impact linear response");
        Require(Near(a.Get().angular, b.Get().angular), "off-centre impact angular response");
        const auto v = a.Get();
        Require(std::abs(glm::dot(f.y, v.linear + glm::cross(v.angular, contactArm)) - 2 * restitution) < 2e-12,
                "analytic normal restitution");
        Require(a.Energy() <= before + 2e-12, "impact cannot create kinetic energy");
        if (restitution == 1) Require(std::abs(a.Energy() - before) < 2e-12, "elastic impact energy");
    }
}

void CameraContracts() {
    const auto view = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0), glm::vec3(0, 1, 0));
    const auto projection = glm::ortho(-8.f, 8.f, -4.f, 4.f, .1f, 100.f);
    const auto centre = ProjectViewport(view, projection, {0, 0, 0});
    const auto corner = ProjectViewport(view, projection, {8, 4, 0});
    Require(glm::length(centre.position - glm::vec2(.5f)) < 1e-6f && centre.inside, "orthographic centre");
    Require(glm::length(corner.position - glm::vec2(1, 0)) < 1e-6f, "top-left viewport convention");
    Require(!ProjectViewport(view, projection, {0, 0, 20}).inside, "orthographic eye-back point excluded by depth");
    const auto inverse = glm::inverse(projection * view);
    auto ray = [&](float x) {
        const auto a = inverse * glm::vec4(x, 0, -1, 1);
        const auto b = inverse * glm::vec4(x, 0, 1, 1);
        return std::array<glm::vec3, 2>{glm::vec3(a) / a.w, glm::normalize(glm::vec3(b) / b.w - glm::vec3(a) / a.w)};
    };
    const auto left = ray(-.5f), right = ray(.5f);
    Require(glm::length(left[1] - right[1]) < 1e-6f, "orthographic rays parallel");
    Require(std::abs(right[0].x - left[0].x - 8) < 1e-6f, "orthographic rays have different origins");
    // Discovery, not a production correction: clip w is always 1 for ortho.
    Require(!ProjectViewport(view, projection, {0, 0, 20}).behind,
            "existing behind flag does not classify orthographic eye-back points");
    Judas2DStage0::Projection camera;
    camera.kind=Judas2DStage0::ProjectionKind::Orthographic;
    Require(camera.Project(view,2,{0,0,20}).behind,"prototype uses signed eye depth for ortho behind flag");
    for(float aspect:{.5f,1.f,2.f}){
        const auto edge=camera.Project(view,aspect,{4*aspect,4,0});
        Require(glm::length(edge.position-glm::vec2(1,0))<1e-6f,"orthographic height/aspect framing");
        for(glm::vec2 pixel:{glm::vec2(.2f,.7f),glm::vec2(.8f,.3f)}){
            const auto r=camera.Unproject(view,aspect,pixel);
            const auto world=r.origin+r.direction*3.f;
            Require(glm::length(camera.Project(view,aspect,world).position-pixel)<1e-5f,"orthographic screen/world roundtrip");
        }
    }
    camera.kind=Judas2DStage0::ProjectionKind::Perspective;
    const auto old=glm::perspective(glm::radians(60.f),2.f,.1f,100.f);
    Require(camera.Matrix(2)==old,"perspective default matrix remains identical");
    Require(camera.Unproject(view,2,{.1f,.5f}).origin==camera.Unproject(view,2,{.9f,.5f}).origin,"perspective ray origins coincide");
    for(int invalid=0;invalid<4;++invalid){
        auto bad=camera;bad.kind=Judas2DStage0::ProjectionKind::Orthographic;
        if(invalid==0)bad.visibleHeight=0;
        if(invalid==1)bad.nearPlane=-1;
        if(invalid==2)bad.farPlane=bad.nearPlane;
        bool rejected=false;
        try{(void)bad.Matrix(invalid==3?0:1);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"invalid camera descriptor rejects");
    }
}

volatile double observed = 0;
template<class Body> double Benchmark(Frame frame) {
    std::vector<Body> bodies;
    for (int i = 0; i < 1024; ++i) bodies.emplace_back(frame, 3, 5);
    const auto begin = std::chrono::steady_clock::now();
    for (int step = 0; step < 100; ++step) {
        for (auto& body : bodies) body.Impulse(.001 * (frame.x + frame.n), .2 * frame.y);
    }
    const auto end = std::chrono::steady_clock::now();
    for (const auto& body : bodies) observed += body.Energy();
    return std::chrono::duration<double, std::milli>(end - begin).count() / 100;
}
}
int main() {
    try {
        const Frame aligned(glm::dquat(1, 0, 0, 0));
        const Frame rotated(glm::angleAxis(.73, glm::normalize(V(1, 2, 3))));
        MobilityContracts(aligned);
        MobilityContracts(rotated);
        CameraContracts();
        std::array<double, 9> reduced{}, rows{};
        for (std::size_t i = 0; i < reduced.size(); ++i) {
            if (i % 2) { rows[i] = Benchmark<Rows>(rotated); reduced[i] = Benchmark<Reduced>(rotated); }
            else { reduced[i] = Benchmark<Reduced>(rotated); rows[i] = Benchmark<Rows>(rotated); }
        }
        std::sort(reduced.begin(), reduced.end());
        std::sort(rows.begin(), rows.end());
        std::printf("{\"checks\":%d,\"failures\":0,\"bodies\":1024,\"scope\":\"impulse response only; not MK2 steps\","
                    "\"reduced_median_ms\":%.9g,\"rows_median_ms\":%.9g,\"reduced_worst_ms\":%.9g,\"rows_worst_ms\":%.9g}\n",
                    checks, reduced[4], rows[4], reduced[8], rows[8]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %d checks: %s\n", checks, error.what());
        return 1;
    }
}
