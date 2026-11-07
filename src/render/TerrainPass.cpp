#include "Renderer.h"
#include "terrain/TerrainLod.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void Renderer::BeginTerrainMaterial(TextureHandle weights,
                                    const std::array<TextureHandle, 4> &layers,
                                    const std::array<glm::vec3, 4> &colours,
                                    const glm::mat4 &worldToTerrain) {
    // Flush unrelated queued materials before changing terrain-only state.
    EndMeshBatch();
    m_terrainMaterial = true;
    m_terrainWeights = weights;
    m_terrainLayers = layers;
    m_terrainColours = colours;
    m_terrainFromWorld = worldToTerrain;
}
void Renderer::BindTerrainMaterial() {
    if (!m_terrainMaterial)
        return;
    glUniform1i(MaterialUniform("uTerrain"), true);
    const char *names[] = {"uMR", "uNormal", "uOcclusion", "uEmissive"};
    // These units normally carry optional material maps, which terrain replaces
    // as one complete material. Environment, water, shadow and noise units stay
    // intact, including devices with the portable sixteen-unit limit.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_terrainWeights));
    glBindSampler(0, 0);
    glUniform1i(MaterialUniform("uTexture"), 0);
    glUniform1i(MaterialUniform("uHasNormal"), false);
    glUniform1i(MaterialUniform("uHasMR"), false);
    glUniform1i(MaterialUniform("uHasOcclusion"), false);
    glUniform1i(MaterialUniform("uHasEmissive"), false);
    for (unsigned i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE5 + i);
        glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_terrainLayers[i]));
        glBindSampler(5 + i, 0);
        glUniform1i(MaterialUniform(names[i]), 5 + i);
    }
    glUniform3fv(MaterialUniform("uTerrainColours[0]"), 4, glm::value_ptr(m_terrainColours[0]));
    glUniformMatrix4fv(MaterialUniform("uTerrainFromWorld"), 1, GL_FALSE,
                       glm::value_ptr(m_terrainFromWorld));
    glActiveTexture(GL_TEXTURE0);
}
void Renderer::EndTerrainMaterial() {
    m_terrainMaterial = false;
    glUseProgram(m_shaderProgram);
    glUniform1i(MaterialUniform("uTerrain"), false);
    glActiveTexture(GL_TEXTURE0);
    BeginMeshBatch();
}

void Renderer::BindTerrainGeometry(bool shadow) {
    if (!m_terrainGrid)
        return;
    const auto program = shadow ? m_shadowShaderProgram : m_shaderProgram;
    auto uniform = [&](const char *name) { return glGetUniformLocation(program, name); };
    glUniform1i(uniform("uTerrainGrid"), true);
    glUniform1i(uniform("uTerrainHeights"), 2);
    glUniform4fv(uniform("uTerrainSamples"), 1, glm::value_ptr(m_terrainSampleRect));
    glUniform4fv(uniform("uTerrainEdges"), 1, glm::value_ptr(m_terrainEdgeStrides));
    glUniform2fv(uniform("uTerrainDimensions"), 1, glm::value_ptr(m_terrainDimensions));
    glUniform1f(uniform("uTerrainSpacing"), m_terrainSpacing);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_terrainHeights));
    glBindSampler(2, 0);
    glActiveTexture(GL_TEXTURE0);
}
void Renderer::DrawTerrainGrid(TextureHandle heights, unsigned cells, glm::vec4 samples,
                               glm::vec4 edges, glm::vec2 dimensions, float spacing,
                               const glm::vec3 &position, const glm::quat &rotation, float width,
                               float minimum, float maximum, const glm::vec3 &colour, float alpha) {
    if (!AcceptsCurrentLayer())
        return;
    VisualBounds bounds;
    bounds.Include({0, minimum, 0});
    bounds.Include({width, maximum, width});
    auto frame = glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation);
    if (!IsVisible(TransformBounds(bounds, frame)))
        return;
    auto &grid = m_terrainGrids[cells];
    if (!grid.IsValid())
        grid = CreateMesh(TerrainV2::HeightGrid(cells));
    m_terrainGrid = true;
    m_terrainHeights = heights;
    m_terrainSampleRect = samples;
    m_terrainEdgeStrides = edges;
    m_terrainDimensions = dimensions;
    m_terrainSpacing = spacing;
    const auto culling = m_cullingEnabled;
    m_cullingEnabled = false; // already tested displaced bounds
    DrawMesh(grid, position, rotation, {width, 1, width}, {}, colour, alpha);
    m_cullingEnabled = culling;
    m_terrainGrid = false;
    const auto program = m_shadowPassActive ? m_shadowShaderProgram : m_shaderProgram;
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTerrainGrid"), false);
}
