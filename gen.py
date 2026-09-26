import subprocess
import argparse
import sys
import os
import time
import shutil

arg_parser = argparse.ArgumentParser("setup")
arg_parser.add_argument("-c",  "--clean",     help="clean all build files",         action="store_true")
arg_parser.add_argument("-v",  "--verbose",   help="print debug information",       action="store_true")
arg_parser.add_argument("-l",  "--list-deps", help="list all current dependencies", action="store_true")
arg_parser.add_argument("-r",  "--release",   help="build in release mode",         action="store_true")

program_args = arg_parser.parse_args()

def log(message, delay = 2, verbose = False):
    if verbose and not program_args.verbose:
        return

    print(f"(setup.py) ------- {message}")
    time.sleep(delay)

def append_args(args, other = None):
    if type(other) == str:
        return args + [ other ]
    elif type(other) == list:
        return args + other

def cmake_command(args):
    command = [ "cmake" ]
    command = append_args(command, args)

    if sys.platform.startswith('win'):
        shell = True
    else:
        shell = False

    log(command, verbose=True)

    code = subprocess.check_call(command, stderr=subprocess.STDOUT, shell=shell)
    if code != 0:
        raise RuntimeError("Command execution failed.")
    return code
class Dependency:
    def __init__(self, name):
        self.name = name
        self.g_args = []
        self.b_args = []
        self.dep_active = True
    def only_on_platform(self, platform_name):
        self.dep_active = (sys.platform == platform_name)
        return self
    def gen_args(self, args):
        self.g_args = append_args(self.g_args, args)
        return self
    def gen_flag(self, flag, platform = None):
        if (platform and (sys.platform == platform)) or platform == None:
            self.g_args = append_args(self.g_args, f"-D{flag}")
        return self
    def build_args(self, args):
        self.b_args = append_args(self.b_args, args)
        return self
    def shared_library(self):
        self.gen_args("-DBUILD_SHARED_LIBS=ON")
        self.is_shared = True
        return self
    def static_library(self):
        self.gen_args("-DBUILD_SHARED_LIBS=OFF")
        self.is_shared = False
        return self
    def dir(self):
        return f"deps/{self.name}"
    def build_dir(self):
        return f"{self.dir()}/build"
    def install_dir(self):
        return f"{self.dir()}/install"
    def is_built(self):
        return os.path.exists(self.install_dir()) or os.path.exists(self.build_dir())
    def configure(self):
        log(f"Configuring '{self.name}'", delay=0)
        log(f"Custom arguments: {self.g_args}")

        args = [ "-S", self.dir(), "-B", self.build_dir() ]
        args.append(f"-DCMAKE_INSTALL_PREFIX={self.install_dir()}")
        args.append(f"-DCMAKE_DEBUG_POSTFIX=d")
        args = append_args(args, self.g_args)
        cmake_command(args)
        return self
    def install(self):
        log(f"Building '{self.name}'...", delay=0)
        log(f"Custom arguments: {self.b_args}")

        args = [ "--build", self.build_dir(), "--target", "install" ]
        args = append_args(args, self.b_args)

        debug_args = [ "--config", "Debug" ]
        release_args = [ "--config", "Release" ]
        cmake_command(append_args(args, debug_args))
        cmake_command(append_args(args, release_args))
        return self
    def clean(self):
        if self.is_built():
            if os.path.exists(self.build_dir()):
                shutil.rmtree(self.build_dir())
            if os.path.exists(self.install_dir()):
                shutil.rmtree(self.install_dir())
        return self
    def setup(self, release):
        if self.dep_active == False:
            log(f"Dependency '{self.name}' is disabled.", delay = 0)
            return self

        if self.is_built():
            log(f"Project '{self.name}' is already built. Skipping...", delay = 0)
            return self

        self.configure()
        self.install()
        return self

dependencies = [
    Dependency('glfw')
        .gen_flag('GLFW_BUILD_EXAMPLES=OFF'),
    Dependency('fastgltf'),
    Dependency('nlohmann-json'),
    Dependency('glm')
        .gen_flag('GLM_BUILD_TESTS=OFF'),
    Dependency('reactphysics3d')
        .gen_flag('CMAKE_CXX_FLAGS_INIT=/FIchrono', platform = "win32")
        .gen_flag('CMAKE_CXX_FLAGS_INIT=-include chrono', platform = "darwin"),
    Dependency('vk-bootstrap')
        .gen_flag("VK_BOOTSTRAP_INSTALL=ON")
        .gen_flag("VK_BOOTSTRAP_TEST=OFF"),
    Dependency('googletest')
        .gen_flag("gtest_force_shared_crt=ON"),
    Dependency('nontype_functional')
        .only_on_platform('darwin')
]

def create_prefix_path():
    res = "-DCMAKE_PREFIX_PATH="
    for dep in dependencies:
        res += f"{dep.install_dir()};"
    return res

if program_args.clean:
    log("Cleaning build files...")
    for dep in dependencies:
        dep.clean()


log(f"Building dependencies... (Release: {program_args.release})")
for dep in dependencies:
    dep.setup(program_args.release)
