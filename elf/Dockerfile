# This was tested on Apple Silicon, and it might not work on other archs.

FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc-x86-64-linux-gnu \
      binutils-x86-64-linux-gnu \
      libc6-dev-amd64-cross \
      file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
RUN printf '#include <stdio.h>\nint main(){puts("hi");return 0;}\n' > hello.c
RUN x86_64-linux-gnu-gcc -g -O0 -o /usr/local/bin/hello-debug hello.c \
 && file /usr/local/bin/hello-debug

RUN pip install --no-cache-dir "sqlelf==0.5" "lief<0.15"

# Silence LIEF nanobind leak warnings + logging
RUN python - <<'PY'
import site, pathlib
p = pathlib.Path(site.getsitepackages()[0]) / "sitecustomize.py"
p.write_text(
    "import lief\n"
    "try: lief.disable_leak_warning()\n"
    "except Exception: pass\n"
    "try:\n"
    "    import lief.logging as L\n"
    "    L.set_level(L.LEVEL.ERROR)\n"
    "except Exception: pass\n"
)
print("Installed", p)
PY

ENTRYPOINT ["sqlelf"]
CMD ["/usr/local/bin/hello-debug"]

