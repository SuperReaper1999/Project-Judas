#include "Renderer.h"
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
    glUniform1i(MaterialUniform("uTerrain"),true);
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
