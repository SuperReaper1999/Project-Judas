# GLAD vendor note

Judas vendors the generated C loader from GLAD 2 **v2.0.8**, upstream tag
`v2.0.8`, source commit
`73db193f853e2ee079bf3ca8a64aa2eaf6459043` in
[`Dav1dde/glad`](https://github.com/Dav1dde/glad).

Generation used Python 3.14, Jinja2 3.1.6, and the `gl.xml` shipped in that
exact GLAD source checkout. From its `glad/files` directory, the command was:

```sh
python3 -m glad --api gl:core=4.6 --extensions GL_ARB_buffer_storage,GL_ARB_compute_shader,GL_ARB_direct_state_access,GL_ARB_multi_draw_indirect,GL_ARB_shader_storage_buffer_object,GL_ARB_texture_view,GL_ARB_shader_draw_parameters,GL_KHR_debug,GL_ARB_robustness \
  --out-path /tmp/judas-glad-generated --reproducible c
```

The generated API/profile is **OpenGL 4.6 Core**, with the nine explicitly listed extensions;
the C generator's internal platform loader, debug hooks, aliases, and
multiple-context mode were not enabled. `gl.xml` SHA-256 in the source
checkout: `5f6729a142ec29f58967381b9efd4bd41ba90749a0f8bffb52bf457d54263461`.

Only the generated files needed to build/load this API are included:

- `include/glad/gl.h`
- `include/KHR/khrplatform.h`
- `src/gl.c`

Renderer V2 negotiates the context and checks capabilities before using modern facilities; OpenGL 3.3 remains a supported portable desktop path.

Judas loads GLAD through its existing SDL-created current context using
`SDL_GL_GetProcAddress`; no GLAD platform loader or runtime generator is
shipped. `LICENSE` is copied intact from the pinned GLAD source checkout.
It retains GLAD's MIT notice, the Khronos OpenGL specification Apache-2.0
notice, and the Khronos header notice. The generated GLAD files carry their
own SPDX notice (`(WTFPL OR CC0-1.0) AND Apache-2.0`); `khrplatform.h` also
retains its original Khronos copyright/license header.
