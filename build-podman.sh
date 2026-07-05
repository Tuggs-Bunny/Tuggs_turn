#!/usr/bin/env bash
# Rebuild tuggs_turn using a rootless Fedora 44 container (no host toolchain needed).
set -e
podman run --rm -v /home/monty/Projects/Tuggs_turn:/src:Z registry.fedoraproject.org/fedora:44 bash -c '
  dnf install -y -q gcc-c++ cmake libX11-devel >/dev/null 2>&1
  cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build /src/build -j"$(nproc)"
'
