// @ts-check
/** @param {import("judas").Entity} camera Existing render-target camera.
 * @param {import("judas").Vec3} point Exit-plane point in the current local world.
 * @param {import("judas").Vec3} normal Plane normal toward the retained side.
 * Call from your presentationUpdate after positioning the camera. */
export function windowProjection(camera, point, normal) {
  return camera.setCameraProjection({near:0.1, far:1000,
    frustum:{left:-0.08,right:0.12,bottom:-0.06,top:0.06},
    clipPlane:{point,normal}});
}
/** @param {import("judas").Entity} camera */
export function symmetricProjection(camera) {
  return camera.setCameraProjection({near:0.1,far:1000,frustum:null,clipPlane:null});
}
