FROM emscripten/emsdk:4.0.15 AS builder

ARG LVGL_COMMIT=c29651105d7c150789b9c53de7dab8bff67daee4
ARG M5GFX_VERSION=0.2.19
ARG M5GFX_COMMIT=53a7184601f3667b030ba141c58b87ce2acfaa2a

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/lvgl/lvgl.git /opt/lvgl \
    && cd /opt/lvgl \
    && git checkout "${LVGL_COMMIT}"

RUN git clone --branch "${M5GFX_VERSION}" --depth 1 https://github.com/m5stack/M5GFX.git /opt/m5gfx \
    && test "$(git -C /opt/m5gfx rev-parse HEAD)" = "${M5GFX_COMMIT}"

WORKDIR /workspace
COPY simulator /workspace/simulator
COPY src /workspace/src
COPY ui /workspace/ui

RUN git -C /opt/m5gfx apply /workspace/simulator/m5gfx/emscripten-panel-sdl.patch

RUN --mount=type=cache,target=/workspace/build,sharing=locked \
    emcmake cmake -S simulator -B build -DLVGL_DIR=/opt/lvgl -DM5GFX_DIR=/opt/m5gfx \
    && cmake --build build --parallel \
    && mkdir -p /workspace/dist \
    && cp build/index.html build/index.data build/m5gfx.html build/m5gfx.js build/m5gfx.wasm /workspace/dist/ \
    && cp build/index.html /workspace/dist/metal.html \
    && cp build/index.html /workspace/dist/cyberdeck.html

FROM nginx:1.27-alpine
COPY --from=builder /workspace/dist/ /usr/share/nginx/html/
EXPOSE 80
