#include "HVVariableCalculate.h"

#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

static int g_failures = 0;

static void Check(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++g_failures;
    }
}

static std::filesystem::path FindRepoRootFromExecutable()
{
    wchar_t module_path[MAX_PATH] = {};
    const DWORD path_len = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    if (path_len == 0 || path_len == MAX_PATH) {
        return std::filesystem::current_path();
    }

    std::filesystem::path current = std::filesystem::path(module_path).parent_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "include" / "HVVariableCalculate.h") &&
            std::filesystem::exists(current / "source" / "LogicTool" / "VariableCalculate" /
                                    "HVVariableCalculate.cpp")) {
            return current;
        }
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

static void TestShape()
{
    HVVariableCalculate calc;

    const std::vector<int> input_types = calc.get_algorithm_input_params_type();
    const std::vector<int> output_types = calc.get_algorithm_output_params_type();
    const std::vector<void*> current_params = calc.get_current_params();

    Check(input_types.size() == 1, "VariableCalculate input count");
    if (input_types.size() >= 1) {
        Check(input_types[0] == HV_STRING_LIST, "VariableCalculate input[0] type");
    }

    Check(current_params.size() == 1, "VariableCalculate current param count");
    Check(output_types.size() == 4, "VariableCalculate output count");
}

static void TestSetExpressionListParam()
{
    HVVariableCalculate calc;

    HVStringList expression_list;
    expression_list.values = { "1+2", "3+4" };
    Check(calc.set_algorithm_params({ &expression_list }, { 0 }) == SUCCESS,
          "VariableCalculate set expression_list param");

    const std::vector<void*> current_params = calc.get_current_params();
    Check(current_params.size() == 1, "VariableCalculate current params after set");
    if (current_params.size() >= 1 && current_params[0] != nullptr) {
        const auto* stored_list = static_cast<const HVStringList*>(current_params[0]);
        Check(stored_list->values.size() == 2, "VariableCalculate stored expression_list size");
        if (stored_list->values.size() >= 2) {
            Check(stored_list->values[0] == "1+2", "VariableCalculate stored expression_list[0]");
            Check(stored_list->values[1] == "3+4", "VariableCalculate stored expression_list[1]");
        }
    }
}

static void TestDynamicLoadShape()
{
    const std::filesystem::path dll_path =
        FindRepoRootFromExecutable() /
        "build" /
        "HV_3DPilot_AlgorithmRelease/plugins/LogicTool/VariableCalculate/VariableCalculate.dll";
    Check(std::filesystem::exists(dll_path), "VariableCalculate dll exists");
    if (!std::filesystem::exists(dll_path)) {
        return;
    }

    HMODULE dll_handle = LoadLibraryW(dll_path.wstring().c_str());
    Check(dll_handle != nullptr, "VariableCalculate LoadLibrary");
    if (dll_handle == nullptr) {
        return;
    }

    using CreateInstanceFn = NodeEngine * (*)();
    const auto create_instance =
        reinterpret_cast<CreateInstanceFn>(GetProcAddress(dll_handle, "CreateInstance"));
    Check(create_instance != nullptr, "VariableCalculate CreateInstance export");
    if (create_instance != nullptr) {
        std::unique_ptr<NodeEngine> instance(create_instance());
        Check(instance != nullptr, "VariableCalculate dynamic instance");
        if (instance != nullptr) {
            const std::vector<int> input_types = instance->get_algorithm_input_params_type();
            Check(input_types.size() == 1, "VariableCalculate dynamic input count");
            if (input_types.size() >= 1) {
                Check(input_types[0] == HV_STRING_LIST, "VariableCalculate dynamic input[0] type");
            }
        }
    }

    FreeLibrary(dll_handle);
}

int main()
{
    std::printf("=== HVVariableCalculate tests ===\n");

    TestShape();
    TestSetExpressionListParam();
    TestDynamicLoadShape();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
