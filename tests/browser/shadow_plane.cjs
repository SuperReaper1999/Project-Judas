// Test the actual production shadow function against independently filled depth
// planes, including real occluders and adjacent tiles selecting different lights.
// PLAYWRIGHT_MODULE=... node shadow_plane.cjs OUT chromium|firefox [Renderer.cpp]
const fs = require('fs'), path = require('path');
const {chromium, firefox} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const [out, name = 'firefox', file = path.join(__dirname, '../../src/Renderer.cpp')] = process.argv.slice(2);
fs.mkdirSync(out, {recursive: true});
const source = fs.readFileSync(file, 'utf8');
const start = source.indexOf('float ComputeShadowFactor(');
const end = source.indexOf('\n}', start) + 2;
if (start < 0 || end < 2) throw Error('Missing production shadow function');
const shadow = source.slice(start, end);
(async () => {
  let browser;
  try {
    browser = await ({chromium, firefox}[name]).launch({headless: true,
      ...(name === 'chromium' ? {channel: 'chromium', args: ['--no-sandbox', '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader']} : {})});
    const page = await browser.newPage();
    const rows = await page.evaluate(shadow => {
      const canvas = document.createElement('canvas'); canvas.width = canvas.height = 64;
      const gl = canvas.getContext('webgl2', {antialias: false});
      if (!gl) throw Error('WebGL2 unavailable');
      function compile(type, text) {
        const shader = gl.createShader(type); gl.shaderSource(shader, text); gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw Error(gl.getShaderInfoLog(shader));
        return shader;
      }
      const vertex = compile(gl.VERTEX_SHADER, `#version 300 es
        precision highp float; out vec2 uv;
        void main(){vec2 p=vec2((gl_VertexID==1)?3.0:-1.0,(gl_VertexID==2)?3.0:-1.0);uv=p*.5+.5;gl_Position=vec4(p,0,1);}`);
      const fragment = compile(gl.FRAGMENT_SHADER, `#version 300 es
        precision highp float; precision highp int; precision highp sampler2DArray;
        uniform sampler2DArray uShadowMaps; uniform mat4 uLightSpaceMatrix[9];
        uniform vec3 plane; uniform bool tiled; in vec2 uv; out vec4 color;
        vec3 shadowWorldDx,shadowWorldDy;
        ${shadow}
        void main(){vec2 xy=uv*2.0-1.0;vec3 p=vec3(xy,dot(plane.xy,xy)+plane.z);
          shadowWorldDx=dFdx(p);shadowWorldDy=dFdy(p);
          int slot=tiled&&uv.x>=.5?1:0;float lit=ComputeShadowFactor(slot,p,1.0);color=vec4(vec3(lit),1);}`);
      const program = gl.createProgram(); gl.attachShader(program, vertex); gl.attachShader(program, fragment); gl.linkProgram(program);
      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw Error(gl.getProgramInfoLog(program));
      gl.deleteShader(vertex); gl.deleteShader(fragment); gl.useProgram(program);
      const vao = gl.createVertexArray(); gl.bindVertexArray(vao);
      const texture = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D_ARRAY, texture);
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.uniform1i(gl.getUniformLocation(program, 'uShadowMaps'), 0);
      const matrix = k => new Float32Array([1,0,0,k, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
      const rows = [];
      for (const [label, a, b, c, k, occlusion, tiled] of [
        ['flat plane',0,0,0,0,0,false],
        ['oblique plane',3.2,1.7,.02,0,0,false],
        ['opposite slope',-2.8,1.3,-.03,0,0,false],
        ['perspective light',2,-.8,.1,.45,0,false],
        ['real occluder',2,-.8,.1,.45,.03,false],
        ['tiled light boundary',2,-.8,.1,0,0,true]
      ]) {
        const depths = new Uint32Array(64 * 64 * 2);
        for (let layer = 0; layer < 2; ++layer) {
          const projective = tiled && layer ? .45 : k;
          for (let y = 0; y < 64; ++y) for (let x = 0; x < 64; ++x) {
            const u = (x+.5)/64*2-1, v = (y+.5)/64*2-1;
            // Independent ray/plane intersection in the inverse light chart.
            const w = 1/(1-projective*u), worldX=u*w, worldY=v*w;
            const z = (a*worldX+b*worldY+c)/w*.5+.5;
            const depth = Math.min(1,Math.max(0,z-occlusion-(tiled&&layer?.03:0)));
            depths[(layer*64+y)*64+x] = Math.round(depth*4294967295);
          }
          gl.uniformMatrix4fv(gl.getUniformLocation(program, `uLightSpaceMatrix[${layer}]`), false, matrix(projective));
        }
        gl.texImage3D(gl.TEXTURE_2D_ARRAY,0,gl.DEPTH_COMPONENT24,64,64,2,0,gl.DEPTH_COMPONENT,gl.UNSIGNED_INT,depths);
        gl.uniform3f(gl.getUniformLocation(program,'plane'),a,b,c);
        gl.uniform1i(gl.getUniformLocation(program,'tiled'),tiled);
        gl.drawArrays(gl.TRIANGLES,0,3);
        const pixels = new Uint8Array(64*64*4); gl.readPixels(0,0,64,64,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
        let bad=0;
        for(let y=30;y<34;++y)for(let x=30;x<34;++x){
          const expected = occlusion>0 || (tiled&&x>=32) ? 0 : 255;
          if(Math.abs(pixels[(y*64+x)*4]-expected)>1)++bad;
        }
        const error=gl.getError(); rows.push({label,badPixels:bad,glError:error});
      }
      gl.deleteTexture(texture); gl.deleteVertexArray(vao); gl.deleteProgram(program);
      return rows;
    }, shadow);
    fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({browser:name,version:browser.version(),source:file,rows},null,2));
    for(const row of rows)console.log(row.badPixels||row.glError?'FAIL':'PASS',JSON.stringify(row));
    if(rows.some(r=>r.badPixels||r.glError))process.exitCode=1;
  } finally { if(browser)await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1});
