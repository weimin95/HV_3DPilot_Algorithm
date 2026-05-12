#pragma once

#include <string>

#include "node_host_services.h"

namespace hvref {

enum class ReferenceKind {
    NodeResult,
    GlobalVariable
};

enum class ResolveError {
    None,
    InvalidSyntax,
    InvalidTypeTag,
    ResolveFailed,
    NodeNotFound,
    NotReachable,
    OutputNotFound,
    Unavailable,
    TypeMismatch,
    EmptyValue,
    UnsupportedType
};

struct ParsedReferenceToken {
    ReferenceKind kind = ReferenceKind::NodeResult;
    int node_id = -1;
    int result_id = -1;
    int global_var_token_id = -1;
    std::string global_var_name;
    int expected_type = -1;
    std::string format_spec;
};

struct ResolvedReferenceValue {
    int type = -1;
    bool has_value = false;
    const void* data = nullptr;
    NodeHostValue owned_global_value;
};

std::string TrimCopy(const std::string& text);
int MapTypeNameToHvType(const std::string& type_name);
std::string FormatFixed4(double value);

ResolveError ParseReferenceToken(const std::string& token_text, ParsedReferenceToken& out_token);
ResolveError ResolveReferenceValue(
    NodeHostServices& host_services,
    const ParsedReferenceToken& token,
    ResolvedReferenceValue& out_value);

ResolveError FormatReferenceValue(const ResolvedReferenceValue& value, std::string& out_text, const std::string& format_spec = "");
ResolveError ConvertReferenceValueToDouble(const ResolvedReferenceValue& value, double& out_value);

}  // namespace hvref
