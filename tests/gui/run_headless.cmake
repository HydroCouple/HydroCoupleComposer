# Drives HydroCoupleComposer --run over a small composition and checks that it
# produced the artefacts the composition asked for.

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

file(WRITE "${WORK_DIR}/composition.json" [=[{
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } },
    { "id": "downstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "connections": [
    { "from": { "component": "upstream", "output": "values" },
      "to": { "component": "downstream", "input": "inflow" } }
  ],
  "writers": [ { "type": "csv", "path": "out/values.csv" } ],
  "run": { "manifest": "out/manifest.json" }
}]=])

execute_process(
    COMMAND "${COMPOSER_BINARY}" --run "${WORK_DIR}/composition.json"
            --components "${FIXTURE_DIR}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE stdout_text
    ERROR_VARIABLE stderr_text)

if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR
        "headless run failed (${exit_code})\n${stdout_text}\n${stderr_text}")
endif()

if(NOT EXISTS "${WORK_DIR}/out/values.csv")
    message(FATAL_ERROR "no CSV was written\n${stdout_text}")
endif()

if(NOT EXISTS "${WORK_DIR}/out/manifest.json")
    message(FATAL_ERROR "no run manifest was written\n${stdout_text}")
endif()

# The artefacts must hold content, not merely exist.
file(SIZE "${WORK_DIR}/out/values.csv" csv_size)
if(csv_size LESS 128)
    message(FATAL_ERROR "the CSV holds no data (${csv_size} bytes)")
endif()

file(READ "${WORK_DIR}/out/manifest.json" manifest_text)
string(FIND "${manifest_text}" "values" values_at)
if(values_at EQUAL -1)
    message(FATAL_ERROR
        "the manifest does not catalogue the recorded output:\n${manifest_text}")
endif()

message(STATUS "headless run produced ${csv_size} bytes of CSV and a manifest")
