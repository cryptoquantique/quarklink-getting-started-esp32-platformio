from os.path import join, isfile
from os import getlogin

# Import the current working construction environment to the `env` variable.
# alias of `env = DefaultEnvironment()`
Import("env")

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-espidf")
PATCHES = {
    'ds_idf': "ds_idf.patch",
    'ds_mbedtls': "ds_mbedtls.patch",
    'mlkem_mbedtls': "mlkem_mbedtls.patch"
}

def apply_patch(patch_file, submodule_dir = ""):
    # Find patch command depending on OS
    if env['HOST_OS'].startswith("win"):
        # OS is Windows, use patch from git
        print("Detected OS: Windows")
        GIT_PATCH_CMD = "C:\\Program Files\\Git\\usr\\bin\\patch.exe"
        if not isfile(GIT_PATCH_CMD):
            USERNAME = getlogin()
            GIT_PATCH_CMD = f"C:\\Users\\{USERNAME}\\AppData\\Local\\Programs\\Git\\usr\\bin\\patch.exe"
            if not isfile(GIT_PATCH_CMD):
                raise SystemExit("'patch' command not found, make sure git is installed on your machine")
    elif env['HOST_OS'].startswith("linux"):
        # OS is Linux
        print("Detected OS: Linux")
        GIT_PATCH_CMD = "patch"
    elif env['HOST_OS'].startswith("darwin"):
        # OS is MacOS
        print("Detected OS: MacOS")
        GIT_PATCH_CMD = "patch"
    else:
        # Others
        GIT_PATCH_CMD = "patch"

    print("Found patch command at %s" % GIT_PATCH_CMD)
        
    print("Project dir: %s" % env["PROJECT_DIR"])
    print(f"Patch file: {patch_file}")
    full_patch = join(env["PROJECT_DIR"], "patches", patch_file)
    if not isfile(full_patch):
        raise SystemExit(f"Patch file {full_patch} not found")

    # patch file only if we didn't do it before
    root_dir = join(FRAMEWORK_DIR,submodule_dir)
    patchflag_path = join(root_dir, f".{patch_file}-done")
    if not isfile(patchflag_path):
        print(f"Applying patch for {full_patch} to {root_dir}")
        rc = env.Execute("\"%s\" -p1 -i %s -d %s" % (GIT_PATCH_CMD, full_patch, root_dir))
        if rc != 0:
            raise SystemExit("Failed to apply patch")

        def _touch(path):
            with open(path, "w") as fp:
                fp.write("")

        env.Execute(lambda *args, **kwargs: _touch(patchflag_path))

    else:
        print("Patch has already been applied")
    
# Apply all patches, always
apply_patch(PATCHES['ds_idf'])
apply_patch(PATCHES['ds_mbedtls'], "components/mbedtls/mbedtls")
apply_patch(PATCHES['mlkem_mbedtls'], "components/mbedtls/mbedtls")