"""Exercise the real message integration helper with a tiny offline producer."""
import os
import pathlib
import subprocess
import tempfile

framework = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="ember-msgs-incremental-") as folder:
    root = pathlib.Path(folder)
    source = root / "source"
    producer = root / "messages"
    build = root / "build"
    source.mkdir()
    producer.mkdir()
    (producer / "proto").mkdir()
    input_file = producer / "proto/revision.proto"
    input_file.write_text("int protocol_revision() { return 1; }\n")
    (producer / "CMakeLists.txt").write_text('''
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bindings.cpp"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/proto/revision.proto" "${CMAKE_CURRENT_BINARY_DIR}/bindings.cpp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/revision.proto")
add_library(openember_msgs_cpp STATIC "${CMAKE_CURRENT_BINARY_DIR}/bindings.cpp")
add_library(openember::msgs_cpp ALIAS openember_msgs_cpp)
set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_IMU_CONTRACT_VERSION 1)
''')
    (source / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.16)
project(message_incremental LANGUAGES CXX)
set(OPENEMBER_SOURCE_DIR "{framework}")
set(OPENEMBER_MSGS_SOURCE LOCAL)
set(OPENEMBER_MSGS_LOCAL_SOURCE "{producer}")
set(OPENEMBER_MSGS_STAGE_DIR_NAME messages)
include("{framework}/cmake/GetOpenEmberMsgs.cmake")
openember_get_openember_msgs()
''')

    def run():
        subprocess.run(["cmake", "-S", str(source), "-B", str(build)], check=True)
        subprocess.run(["cmake", "--build", str(build)], check=True)

    run()
    bindings = build / "_deps/messages-build/bindings.cpp"
    archive = build / "_deps/messages-build/libopenember_msgs_cpp.a"
    before = (bindings.stat().st_mtime_ns, archive.stat().st_mtime_ns)
    run()
    assert before == (bindings.stat().st_mtime_ns, archive.stat().st_mtime_ns), "configure discarded generated files or objects"
    old_time = input_file.stat()
    input_file.write_text("int protocol_revision() { return 2; }\n")
    # Archive replacement can preserve old timestamps: source identity must still invalidate.
    os.utime(input_file, ns=(old_time.st_atime_ns, old_time.st_mtime_ns))
    run()
    assert "return 2" in bindings.read_text(), "changed protocol input did not regenerate bindings"
    assert archive.stat().st_mtime_ns != before[1], "changed bindings did not rebuild the library"
    print("Message incremental configure/build regression passed.")
