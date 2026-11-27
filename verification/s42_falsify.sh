#!/bin/bash
# S4.2 falsifier — canvas binding edges + removal semantics.
set -u
cd "$(dirname "$0")/.."
export QT_QPA_PLATFORM=offscreen
BUILD="cmake --build build/darwin -j 8"
RUN1="./build/darwin/tests/gui/test_compositioncanvas"
RUN2="./build/darwin/tests/gui/test_compositiondocument"

verdict() { # name src
    if ! $BUILD >/dev/null 2>&1; then echo "M$1: BUILD FAILED"; return; fi
    if $RUN1 >/dev/null 2>&1 && $RUN2 >/dev/null 2>&1; then echo "M$1: SURVIVED"; else echo "M$1: CAUGHT"; fi
}

mutate() { # src expr
    local src=$1 expr=$2
    cp "$src" "$src.s42bak"
    perl -0pi -e "$expr" "$src"
    if cmp -s "$src" "$src.s42bak"; then rm -f "$src.s42bak"; return 1; fi
    touch "$src"
    return 0
}

unmutate() {
    local src=$1
    cp "$src.s42bak" "$src"; rm -f "$src.s42bak"; touch "$src"
}

SCENE=src/canvas/compositionscene.cpp
DOC=src/project/compositiondocument.cpp

# M1 binding edges never drawn
mutate $SCENE 's/for \(const HydroCouple::SDK::IO::ArgumentBindingSpec &binding :\n         HydroCouple::SDK::IO::argumentBindings\(m_document->spec\(\)\)\)/for (const HydroCouple::SDK::IO::ArgumentBindingSpec \&binding :\n         std::vector<HydroCouple::SDK::IO::ArgumentBindingSpec>{})/s' \
    && { verdict 1; unmutate $SCENE; } || echo "M1: NOT APPLIED"

# M2 binding edges never follow a moved node
mutate $SCENE 's/    for \(BindingEdgeItem \*edge : m_bindingEdges\)\n    \{\n      edge->refresh\(\);\n    \}//s' \
    && { verdict 2; unmutate $SCENE; } || echo "M2: NOT APPLIED"

# M3 removing a provider leaves its bindings dangling
mutate $DOC 's/if \(HydroCouple::SDK::IO::isArgumentBinding\(it\.value\(\)\) &&\n            it\.value\(\)\[HydroCouple::SDK::IO::kArgumentBindingKey\]\n                \.value\("component", std::string\(\)\) == id\)/if (false)/s' \
    && { verdict 3; unmutate $DOC; } || echo "M3: NOT APPLIED"

# M4 removal strips EVERY binding, not just the removed provider's
mutate $DOC 's/\.value\("component", std::string\(\)\) == id\)/.value("component", std::string()) != "")/s' \
    && { verdict 4; unmutate $DOC; } || echo "M4: NOT APPLIED"

$BUILD >/dev/null 2>&1
