#pragma once
#include "GraphicsApi.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace JudasRender {
// A bounded description of material-owned uniform writes. Texture bindings and
// global raster state remain outside this cache. Names are owned: no resource,
// temporary string or caller lifetime is retained.
class MaterialUniformBatch {
public:
    enum class Kind : unsigned char { Integer, Scalar, Vector3, Vector4, Matrix3 };
    void Int(const char* name, GLint value) { Append(name, Kind::Integer, nullptr, 0).integer=value; }
    void Float(const char* name, GLfloat value) { Append(name, Kind::Scalar, &value, 1); }
    void Vec3(const char* name, const GLfloat* value) { Append(name, Kind::Vector3, value, 3); }
    void Vec4(const char* name, const GLfloat* value) { Append(name, Kind::Vector4, value, 4); }
    void Mat3(const char* name, const GLfloat* value) { Append(name, Kind::Matrix3, value, 9); }

    bool Same(const MaterialUniformBatch& other) const {
        if (m_count != other.m_count || m_values != other.m_values) return false;
        if (std::memcmp(m_data.data(), other.m_data.data(), m_values * sizeof(float))) return false;
        for (unsigned i = 0; i < m_count; ++i) {
            const auto& a = m_writes[i]; const auto& b = other.m_writes[i];
            if (a.kind != b.kind || a.offset != b.offset || a.integer != b.integer || std::strcmp(a.name.data(), b.name.data())) return false;
        }
        return true;
    }
    // Visits every write as (name, kind, integer value, float values). Used by
    // backends that address shader parameters by index instead of GL location.
    template<class Visit> void ForEach(Visit&& visit) const {
        for (unsigned i = 0; i < m_count; ++i) {
            const auto& write = m_writes[i];
            visit(write.name.data(), write.kind, write.integer, m_data.data() + write.offset);
        }
    }
    template<class Resolve> void Publish(Resolve&& location) const {
        for (unsigned i = 0; i < m_count; ++i) {
            const auto& write = m_writes[i];
            const auto uniform = location(write.name.data());
            const auto* value = m_data.data() + write.offset;
            switch (write.kind) {
            case Kind::Integer: glUniform1i(uniform, write.integer); break;
            case Kind::Scalar: glUniform1f(uniform, value[0]); break;
            case Kind::Vector3: glUniform3fv(uniform, 1, value); break;
            case Kind::Vector4: glUniform4fv(uniform, 1, value); break;
            case Kind::Matrix3: glUniformMatrix3fv(uniform, 1, GL_FALSE, value); break;
            }
        }
    }
private:
    struct Write { std::array<char,32> name{}; Kind kind{}; unsigned short offset=0; GLint integer=0; };
    std::array<Write,64> m_writes{};
    std::array<GLfloat,96> m_data{};
    unsigned m_count=0,m_values=0;
    Write& Append(const char* name, Kind kind, const void* data, unsigned values) {
        const auto length = std::strlen(name);
        if (m_count == m_writes.size() || length >= m_writes[0].name.size() || m_values + values > m_data.size())
            throw std::logic_error("material uniform description exceeds its declared shader layout");
        auto& write = m_writes[m_count++];
        std::memcpy(write.name.data(), name, length + 1);
        write.kind = kind; write.offset = static_cast<unsigned short>(m_values);
        if(values)std::memcpy(m_data.data() + m_values, data, values * sizeof(GLfloat));
        m_values += values;
        return write;
    }
};

struct MaterialUniformCache {
    MaterialUniformBatch values;
    GLuint program=0;
    bool valid=false;
    void Invalidate() { valid=false; }
    template<class Resolve> bool Publish(const MaterialUniformBatch& next, GLuint shader, Resolve&& location) {
        if (valid && program == shader && values.Same(next)) return false;
        // If lookup/allocation throws partway through publication, never claim
        // the old values still describe the current GL program.
        valid=false;
        next.Publish(location);
        values=next; program=shader; valid=true;
        return true;
    }
};
}
