#!/usr/bin/env bash
#
# Compiles every Assets/Shaders/*.slang to all backend targets, into
# Assets/Shaders/generated/ (gitignored):
#
#   OpenGL : <name>.glsl        - combined "#type" GLSL 4.10 (Slang -> SPIR-V ->
#                                 SPIRV-Cross), macOS-4.1-core valid, loaded by
#                                 OpenGLShader.
#   Vulkan : <name>.<entry>.spv - SPIR-V per entry point.
#   Metal  : <name>.metal       - MSL, whole module.
#
# Entry points are auto-discovered from Slang [shader("stage")] attributes.
# Override tool paths with SLANGC / SPIRV_CROSS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SLANGC="${SLANGC:-$ROOT/Tools/slang/bin/slangc}"
SPIRV_CROSS="${SPIRV_CROSS:-$(command -v spirv-cross || echo spirv-cross)}"
SRC="$ROOT/Assets/Shaders"
OUT="$SRC/generated"

[ -x "$SLANGC" ] || { echo "compile-shaders: slangc not found at $SLANGC (run Tools/fetch-slang.sh)"; exit 1; }
command -v "$SPIRV_CROSS" >/dev/null 2>&1 || { echo "compile-shaders: spirv-cross not found (brew install spirv-cross)"; exit 1; }

mkdir -p "$OUT"
shopt -s nullglob
shaders=("$SRC"/*.slang)
[ ${#shaders[@]} -gt 0 ] || { echo "compile-shaders: no .slang files in $SRC yet"; exit 0; }

# Prints "stage entryName" for each [shader("stage")] entry point in a file.
entry_points() {
  awk '
    /\[shader\(/ { if (match($0, /"[a-zA-Z]+"/)) { stage = substr($0, RSTART+1, RLENGTH-2); want = 1 } next }
    want && $0 ~ /^[ \t]*\[/ { next }              # skip further attributes
    want && /\(/ {
      sig = $0; sub(/\(.*/, "", sig)
      n = split(sig, t, /[ \t*&]+/); name = t[n]
      if (name != "") print stage, name
      want = 0
    }
  ' "$1"
}

fail=0
for f in "${shaders[@]}"; do
  name="$(basename "$f" .slang)"
  echo "== $name.slang =="

  # Metal: whole module.
  if "$SLANGC" "$f" -target metal -o "$OUT/$name.metal" 2>"$OUT/.err"; then
    echo "   metal  -> $name.metal"
  else echo "   metal  FAILED:"; sed 's/^/     /' "$OUT/.err"; fail=1; fi

  # OpenGL + Vulkan: per entry point. Shaders that declare a ConstantBuffer keep
  # real UBO blocks (the Engine binds them by name); the rest have Slang's
  # $Globals flattened to loose uniforms so per-draw glUniform* calls still work.
  if grep -q 'ConstantBuffer<' "$f"; then ubo=1; else ubo=0; fi
  combined="$OUT/$name.glsl"; : > "$combined"
  while read -r stage entry; do
    [ -z "${stage:-}" ] && continue
    spv="$OUT/$name.$entry.spv"

    if ! "$SLANGC" "$f" -target spirv -entry "$entry" -stage "$stage" -o "$spv" 2>"$OUT/.err"; then
      echo "   spirv  FAILED ($entry):"; sed 's/^/     /' "$OUT/.err"; fail=1; continue
    fi
    echo "   spirv  -> $name.$entry.spv ($stage)"

    # SPIR-V -> classic GLSL 4.10 (combined samplers, no binding layouts).
    xcopts="--version 410 --no-es --no-420pack-extension"
    [ "$ubo" = 0 ] && xcopts="$xcopts --glsl-emit-ubo-as-plain-uniforms"

    if "$SPIRV_CROSS" $xcopts "$spv" --output "$OUT/.stage.glsl" 2>"$OUT/.err"; then
      if [ "$ubo" = 1 ]; then
        # Real UBOs: strip SPIRV-Cross's "_std140" block-name suffix so the block
        # names match glGetUniformBlockIndex("Camera") etc. in the Engine.
        perl -i -pe 's/_std140//g' "$OUT/.stage.glsl"
      else
        # Flatten "struct S {..}; uniform S inst;" into loose "uniform TYPE NAME;"
        # and strip the "inst." prefix, so uniforms keep their original names and
        # the existing C++ SetMat4("u_X") / SetFloat("u_Y") calls work unchanged.
        perl -0777 -i -pe '
          while (/struct\s+(\w+)\s*\{(.*?)\}\s*;\s*uniform\s+\1\s+(\w+)\s*;/s) {
            my ($s,$body,$inst)=($1,$2,$3); my @u;
            while ($body =~ /([A-Za-z_]\w*)\s+([A-Za-z_]\w*(?:\[\d+\])?)\s*;/g) { push @u,"uniform $1 $2;"; }
            my $loose=join("\n",@u);
            s/struct\s+\Q$s\E\s*\{.*?\}\s*;\s*uniform\s+\Q$s\E\s+\Q$inst\E\s*;/$loose/s;
            s/\b\Q$inst\E\.//g;
          }
        ' "$OUT/.stage.glsl"
      fi
      # SPIRV-Cross names inter-stage varyings per stage and relies on explicit
      # locations, but macOS's GL linker matches varyings by NAME. Rename each
      # varying to a location-canonical name (_v<loc>) so stages agree: rename
      # `out` in vertex stages and `in` in fragment stages (never attributes).
      case "$stage" in vertex) vdir=out ;; fragment) vdir=in ;; *) vdir="" ;; esac
      if [ -n "$vdir" ]; then
        DIR="$vdir" perl -0777 -i -pe '
          my $d=$ENV{DIR}; my %m;
          while (/layout\(location = (\d+)\)\s+\Q$d\E\s+\w+\s+(\w+)\s*;/g) { $m{$2}="_v$1"; }
          for my $n (keys %m) { s/\b\Q$n\E\b/$m{$n}/g; }
        ' "$OUT/.stage.glsl"
      fi
      { echo "#type $stage"; cat "$OUT/.stage.glsl"; echo; } >> "$combined"
      echo "   glsl   -> $name.glsl (#type $stage)"
    else echo "   glsl   FAILED ($entry):"; sed 's/^/     /' "$OUT/.err"; fail=1; fi
  done < <(entry_points "$f")
done

rm -f "$OUT/.err" "$OUT/.stage.glsl"
exit $fail
