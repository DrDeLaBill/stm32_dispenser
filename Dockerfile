ARG CI_REGISTRY=registry.izhpt.com:443/ayratproject/stm_builder:main

FROM $CI_REGISTRY AS builder

ARG BUILD_VERSION="v0.0.0"
ARG BUILD_DATE=""

ENV APP_ROOT=/app
ENV SRC_ROOT=$APP_ROOT/src
ENV DEBUG_ROOT=$APP_ROOT/Debug
ENV RELEASE_ROOT=$APP_ROOT/Release

ADD . $SRC_ROOT
RUN mkdir -p $DEBUG_ROOT \
 && mkdir -p $RELEASE_ROOT \
 && cp -f $APP_ROOT/CMakeLists.txt $SRC_ROOT/CMakeLists.txt \
 && cp -f $APP_ROOT/search.cmake $SRC_ROOT/search.cmake

RUN cd $DEBUG_ROOT \
 && cmake -G"Unix Makefiles" $SRC_ROOT --log-level=WARNING -DDEBUG=1 -DPROJECT_NAME=debug -DBUILD_VERSION="$BUILD_VERSION" \
 && cmake --build $DEBUG_ROOT > /dev/null 2>&1

RUN cd $RELEASE_ROOT \
 && cmake -G"Unix Makefiles" $SRC_ROOT --log-level=WARNING -DPROJECT_NAME=release -DBUILD_VERSION="$BUILD_VERSION" \
 && cmake --build $RELEASE_ROOT > /dev/null 2>&1

FROM scratch

ENV APP_ROOT=/app
ENV SRC_ROOT=$APP_ROOT/src
ENV DEBUG_ROOT=$APP_ROOT/Debug
ENV RELEASE_ROOT=$APP_ROOT/Release

COPY --from=builder $DEBUG_ROOT/debug.bin /
COPY --from=builder $DEBUG_ROOT/debug.elf /
COPY --from=builder $RELEASE_ROOT/release.bin /
COPY --from=builder $RELEASE_ROOT/release.elf /


