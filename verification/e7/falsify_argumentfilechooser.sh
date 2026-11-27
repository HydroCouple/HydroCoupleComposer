#!/bin/bash
# Falsification: a file argument's chooser.
#
# The naive version of this slice is a QLineEdit holding a path that gets
# written to the argument as if the path were the value. Every mutation
# below is a form of that mistake, or of forgetting that the composition
# document has exactly one channel to an argument and it is JSON.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_configurator
LOG=verification/e7/falsify_argumentfilechooser.log
: > "$LOG"

FILES=(src/configurator/argumentdescriptor.cpp
       src/configurator/componentconfigurator.cpp
       src/layers/gdalrasterlayer.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  local changed=0
  for f in "${FILES[@]}"; do cmp -s "$f" "$f.orig" || changed=1; done
  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"; return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  # Both targets: a partial --target build leaves the other test binary
  # linked against the code from before the mutation, and it passes.
  if ! cmake --build $BUILD --target test_configurator test_layerpersistence -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1; $BUILD/tests/gui/test_layerpersistence --gtest_filter='*NamesTheFile*' 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ConfiguratorTest' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── is a path a value ───────────────────────────────────────────────────────

# 1. Hand the path to the component as JSON, which is what the payload path
#    already does -- the mistake this whole slice exists to avoid.
m_path_as_json() {
  perl -0pi -e 's/          remote \? HydroCouple::IArgument::ArgumentInputType::URL\n                 : HydroCouple::IArgument::ArgumentInputType::File,/          HydroCouple::IArgument::ArgumentInputType::JSON,/' src/configurator/argumentdescriptor.cpp
}

# 2. Hand the path over as a plain string value instead of as a file to read.
m_path_as_string() {
  perl -0pi -e 's/          remote \? HydroCouple::IArgument::ArgumentInputType::URL\n                 : HydroCouple::IArgument::ArgumentInputType::File,/          HydroCouple::IArgument::ArgumentInputType::String,/' src/configurator/argumentdescriptor.cpp
}

# 3. Record the path in the document rather than what the component read out
#    of the file. It reopens as a component that was handed a filename as if
#    it were data.
m_record_the_path() {
  perl -0pi -e 's/    const nlohmann::json payload = readArgumentPayload\(target, message\);/    const nlohmann::json payload = reference.toStdString();/' src/configurator/componentconfigurator.cpp
}

# 4. Read the payload back before the file is applied, so the document keeps
#    the values the argument had and the component keeps the new ones.
m_record_stale_values() {
  perl -0pi -e 's/    if \(!writeArgumentReference/    const nlohmann::json early = readArgumentPayload(target, message);\n    if (!writeArgumentReference/' src/configurator/componentconfigurator.cpp
  perl -0pi -e 's/    const nlohmann::json payload = readArgumentPayload\(target, message\);/    const nlohmann::json payload = early;/' src/configurator/componentconfigurator.cpp
}

# ── is a refusal a refusal ──────────────────────────────────────────────────

# 5. Take the component's word for nothing: report every load as a success.
m_never_refuse() {
  perl -0pi -e 's/    if \(!argument->initialize\(\n          target,\n          remote \? HydroCouple::IArgument::ArgumentInputType::URL\n                 : HydroCouple::IArgument::ArgumentInputType::File,\n          failure\)\)/    if ((void)argument->initialize(target, remote ? HydroCouple::IArgument::ArgumentInputType::URL : HydroCouple::IArgument::ArgumentInputType::File, failure), false)/' src/configurator/argumentdescriptor.cpp
}

# 6. Throw away the component's reason, so "cannot open" and "not JSON" are
#    the same unhelpful sentence and the user is not told which file.
m_lose_the_reason() {
  perl -0pi -e 's/      message = failure\.empty\(\)\n                  \? QStringLiteral\("the component could not read it"\)\n                  : QString::fromStdString\(failure\);/      message = QStringLiteral("the component could not read it");/' src/configurator/argumentdescriptor.cpp
}

# 7. Record the argument even when the file was refused.
m_record_anyway() {
  perl -0pi -e 's/      m_status->setText\(tr\("\x27%1\x27 rejected \x27%2\x27: %3"\)\n                          \.arg\(argumentId, reference, message\)\);\n      return false;/      m_status->setText(tr("\x27%1\x27 rejected \x27%2\x27: %3")\n                          .arg(argumentId, reference, message));/' src/configurator/componentconfigurator.cpp
}

# ── does the argument's own advertisement reach the dialog ──────────────────

# 8. Offer every file, discarding the filters -- which is what the code did
#    before this slice, only more honestly.
m_drop_the_filters() {
  perl -0pi -e 's/    QStringList entries = fileFilters;/    QStringList entries;/' src/configurator/argumentdescriptor.cpp
}

# 9. Offer only what the argument named, so a file the user knows is right
#    but whose extension is wrong cannot be reached at all.
m_no_all_files() {
  perl -0pi -e 's/    entries\.append\(QStringLiteral\("All Files \(\*\)"\)\);//' src/configurator/argumentdescriptor.cpp
}

# 10. Put All Files first, so the dialog opens showing everything and the
#     argument's own filter is a thing to go looking for.
m_all_files_first() {
  perl -0pi -e 's/    QStringList entries = fileFilters;\n    entries\.append\(QStringLiteral\("All Files \(\*\)"\)\);/    QStringList entries{QStringLiteral("All Files (*)")};\n    entries.append(fileFilters);/' src/configurator/argumentdescriptor.cpp
}

# ── is the chooser a chooser ────────────────────────────────────────────────

# 11. Name the path box the way a lone text editor would be named, which is
#     what a half-done split leaves behind.
m_no_browse_button() {
  perl -0pi -e 's/          browse->setObjectName\(QStringLiteral\("argument_"\) \+ descriptor\.id \+\n                                QStringLiteral\("_browse"\)\);//' src/configurator/componentconfigurator.cpp
}

# 12. Make the box decoration: a path typed and committed does nothing, and
#     only the dialog works.
m_typing_does_nothing() {
  perl -0pi -e 's/                    QString message;\n                    applyArgumentReference\(id, line->text\(\), message\);/                    QString message;/' src/configurator/componentconfigurator.cpp
}

# ── where a reference comes from ────────────────────────────────────────────

# 13. Offer no way to read an argument from the map, which is the one click
#     this whole program exists to make possible.
m_no_layer_button() {
  perl -0pi -e 's/          fromLayer->setObjectName\(QStringLiteral\("argument_"\) \+ descriptor\.id \+\n                                   QStringLiteral\("_layer"\)\);//' src/configurator/componentconfigurator.cpp
}

# 14. Treat every reference as a local path, so an address with a scheme is
#     handed to open(2) and fails as a missing file.
m_everything_is_a_path() {
  perl -0pi -e 's/    const bool remote =\n      !HydroCouple::SDK::IO::uriScheme\(target\)\.empty\(\);/    const bool remote = false;/' src/configurator/argumentdescriptor.cpp
}

# 15. Treat every reference as a URI, so C:\\data\\x.json goes looking for a
#     resolver for the "c" scheme.
m_everything_is_a_uri() {
  perl -0pi -e 's/    const bool remote =\n      !HydroCouple::SDK::IO::uriScheme\(target\)\.empty\(\);/    const bool remote = true;/' src/configurator/argumentdescriptor.cpp
}

# 16. Let a file layer say nothing about where it came from, so it cannot be
#     offered to an argument at all.
m_file_layer_has_no_source() {
  perl -0pi -e 's/    layer->setSourceUri\(QUrl::fromLocalFile\(filePath\)\);//' src/layers/gdalrasterlayer.cpp
}

run_mutation 1-path-as-json          m_path_as_json
run_mutation 2-path-as-string        m_path_as_string
run_mutation 3-record-the-path       m_record_the_path
run_mutation 4-record-stale-values   m_record_stale_values
run_mutation 5-never-refuse          m_never_refuse
run_mutation 6-lose-the-reason       m_lose_the_reason
run_mutation 7-record-anyway         m_record_anyway
run_mutation 8-drop-the-filters      m_drop_the_filters
run_mutation 9-no-all-files          m_no_all_files
run_mutation 10-all-files-first      m_all_files_first
run_mutation 11-no-browse-button     m_no_browse_button
run_mutation 12-typing-does-nothing  m_typing_does_nothing
run_mutation 13-no-layer-button      m_no_layer_button
run_mutation 14-everything-is-a-path m_everything_is_a_path
run_mutation 15-everything-is-a-uri  m_everything_is_a_uri
run_mutation 16-file-layer-no-source m_file_layer_has_no_source

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
