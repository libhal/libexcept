# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd
from pathlib import Path

required_conan_version = ">=2.0.14"


class except_conan(ConanFile):
    name = "libexcept"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/libhal/libexcept"
    description = (
        "Exception handling runtime support for the libhal ecosystem.")
    topics = ("exceptions", "error", "terminate", "unexpected")
    settings = "compiler", "build_type", "os", "arch"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualBuildEnv"
    exports_sources = ("include/*", "tests/*", "LICENSE",
                       "CMakeLists.txt", "runtime/*", "nearpoint/*",
                       "linker_scripts/*")

    @property
    def _min_cppstd(self):
        return "20"

    @property
    def _is_arm_cortex(self):
        return str(self.settings.arch).startswith("cortex-")

    def validate(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, self._min_cppstd)

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.38.0 <5.0.0]")
        self.test_requires("boost-ext-ut/2.1.0")

    def requirements(self):
        self.requires("libhal-util/[^5.0.0]")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self,
             "LICENSE",
             dst=Path(self.package_folder) / "licenses",
             src=self.source_folder)
        copy(self,
             "*.h",
             dst=Path(self.package_folder) / "include",
             src=Path(self.source_folder) / "include")
        copy(self,
             "*.hpp",
             dst=Path(self.package_folder) / "include",
             src=Path(self.source_folder) / "include")
        copy(self,
             "*.ld",
             dst=Path(self.package_folder) / "linker_scripts",
             src=Path(self.source_folder) / "linker_scripts")

        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "libexcept::libexcept")
        self.cpp_info.defines = ["LIBEXCEPT=1"]
        self.cpp_info.exelinkflags.extend([
            "-fexceptions",
            "-L" + str(Path(self.package_folder) / "linker_scripts"),
            "-Wl,--wrap=__cxa_throw",
            "-Wl,--wrap=__cxa_rethrow",
            "-Wl,--wrap=__cxa_end_catch",
            "-Wl,--wrap=__cxa_begin_catch",
            "-Wl,--wrap=__cxa_end_cleanup",
            "-Wl,--wrap=_Unwind_Resume",
        ])

        # CMakeLists.txt builds libexcept as an OBJECT library and installs
        # the loose .o files (no .a, no CMake package config of our own -
        # Conan synthesizes the libexcept::libexcept target for consumers
        # from this cpp_info directly). Passing the .o files themselves on
        # the link line makes the linker include them unconditionally, same
        # as -Wl,--whole-archive used to force for a .a - so that flag isn't
        # needed here either.
        # CMake's OBJECTS DESTINATION install nests these under
        # lib/objects-<CONFIG>/<target>/<source-relative-path>.o(bj), to
        # keep object files with the same basename from different source
        # subdirectories from colliding, so this has to search recursively
        # rather than just listing lib/ itself.
        object_dir = Path(self.package_folder) / "lib"
        self.cpp_info.exelinkflags.extend(
            sorted(str(p) for p in object_dir.rglob("*")
                   if p.is_file() and p.suffix in (".o", ".obj")))
