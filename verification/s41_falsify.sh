#!/bin/bash
# S4.1 falsifier — the configurator's @from binding chooser.
set -u
cd "$(dirname "$0")/.."
export QT_QPA_PLATFORM=offscreen
SRC=src/configurator/componentconfigurator.cpp
BAK=$SRC.s41bak
BUILD="cmake --build build/darwin -j 8"
RUN="./build/darwin/tests/gui/test_configurator"

cp "$SRC" "$BAK"
restore() { cp "$BAK" "$SRC"; }
trap 'restore; rm -f "$BAK"; touch "$SRC"; $BUILD >/dev/null 2>&1' EXIT

verdict() {
    if ! $BUILD >/dev/null 2>&1; then echo "M$1: BUILD FAILED"; return; fi
    if $RUN >/dev/null 2>&1; then echo "M$1: SURVIVED"; else echo "M$1: CAUGHT"; fi
}

mutate() {
    restore
    perl -0pi -e "$1" "$SRC"
    if cmp -s "$SRC" "$BAK"; then return 1; fi
    touch "$SRC"
    return 0
}

# M1 a binding records a bare value payload instead of the reference
mutate 's/if \(!m_document->setArgument\(m_componentId, argumentId, payload\)\)\n    \{\n      message = tr\("could not record the binding/if (!m_document->setArgument(m_componentId, argumentId, nlohmann::json{{"values", nlohmann::json::array()}}))\n    {\n      message = tr("could not record the binding/s' \
    && verdict 1 || echo "M1: NOT APPLIED"

# M2 an unknown provider output is accepted
mutate 's/if \(!known\)\n    \{\n      message = tr\("'"'"'%1'"'"' has no output/if (false)\n    {\n      message = tr("'"'"'%1'"'"' has no output/s' \
    && verdict 2 || echo "M2: NOT APPLIED"

# M3 a component may bind to itself
mutate 's/if \(providerId == m_componentId\)/if (false)/' \
    && verdict 3 || echo "M3: NOT APPLIED"

# M4 the chooser offers the component its own outputs
mutate 's/      if \(componentId == m_componentId\)\n      \{\n        continue; \/\/ A component cannot provide its own initialization\.\n      \}//s' \
    && verdict 4 || echo "M4: NOT APPLIED"

# M5 the provenance chip never renders (anchored on code, not the
# em-dashed comment above it)
mutate 's/          if \(m_document\)\n          \{\n            const std::optional<CompositionDocument::ComponentSpec> spec =/          if (false)\n          {\n            const std::optional<CompositionDocument::ComponentSpec> spec =/s' \
    && verdict 5 || echo "M5: NOT APPLIED"
