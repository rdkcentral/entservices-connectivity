---
applyTo: "CMakeLists.txt"
---

### Requirement

### Coverity Scan Inclusion and Test Workflow Updates for New Plugins

When adding a new plugin in `CMakeLists.txt`, you **must** also update the following files to ensure the plugin is included in required test and Coverity analysis workflows.

Note: in this repository, `cov_build.sh` is intentionally L1-only for native/Coverity runs. Do not enable `RDK_SERVICE_L2_TEST` in `cov_build.sh`; keep L2 coverage in dedicated L2 workflow files.

- **CI Workflow Files:**  
  - `L1-tests.yml`
  - `L2-tests.yml`
- **Coverity Build Script:**  
  - `cov_build.sh`

**Example:**

1. **CMake Plugin Registration Example**

   If you add your plugin in `CMakeLists.txt` as:
   ```cmake
   if (PLUGIN_RESOURCEMANAGER)
       add_subdirectory(ResourceManager)
   endif()
   if (PLUGIN_MY_NEW_PLUGIN)
       add_subdirectory(MyNewPlugin)
   endif()
   ```
2. **Update Coverity Build Script**

   Add your plugin’s flag in the build command in `cov_build.sh`:
   ```bash
   cmake \
       -DPLUGIN_CORE=ON \
       -DPLUGIN_LEGACY=ON \
       -DPLUGIN_MY_NEW_PLUGIN=ON \  # <-- NEW PLUGIN FLAG
   ```
   This ensures Coverity runs on your new plugin.

3. **Update Test Workflow YAMLs**

   Ensure each test workflow references your new plugin using the **DPLUGIN_<PLUGINNAME>** CMake flag in their build/test step. For example, in `L1-tests.yml`:
   ```yaml
   jobs:
     build-test:
       runs-on: ubuntu-22.04
       steps:
         - name: Configure with new plugin
           run: |
             cmake \
               -DPLUGIN_CORE=ON \
               -DPLUGIN_MY_NEW_PLUGIN=ON \
               .
         - name: Run tests
           run: |
             ctest
   ```
  Repeat similar additions in `L2-tests.yml`.

**Summary:**  
Whenever a new plugin is registered via `CMakeLists.txt`, always update:
- `cov_build.sh` (add plugin flag to Coverity scan build step; keep Coverity scope L1-only)
- All applicable test CI workflows (`L1-tests.yml`, `L2-tests.yml`) to include your plugin flag to ensure your plugin undergoes proper code quality checks and testing.
