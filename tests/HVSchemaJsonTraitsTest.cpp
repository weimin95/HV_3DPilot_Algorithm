#include "HVSchemaNodeEngine.h"

#include <cstdio>
#include <cstdlib>
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

static void TestStringListRoundTrip()
{
    HVStringList original;
    original.values = { "hello", "world", "" };

    nlohmann::json json_value;
    Check(HVFieldJsonTraits<HVStringList>::Serialize(original, json_value),
          "StringList serialize");
    Check(json_value.is_array(), "StringList json is array");
    Check(json_value.size() == 3, "StringList json size");

    HVStringList restored;
    Check(HVFieldJsonTraits<HVStringList>::Deserialize(json_value, restored),
          "StringList deserialize");
    Check(restored.values == original.values, "StringList round-trip content");
}

static void TestStringListRejectsNonArray()
{
    HVStringList out;
    Check(!HVFieldJsonTraits<HVStringList>::Deserialize(nlohmann::json("not_array"), out),
          "StringList rejects string");
    Check(!HVFieldJsonTraits<HVStringList>::Deserialize(nlohmann::json(42), out),
          "StringList rejects integer");
    Check(!HVFieldJsonTraits<HVStringList>::Deserialize(nlohmann::json(true), out),
          "StringList rejects bool");
}

static void TestStringListRejectsNonStringItems()
{
    nlohmann::json bad_array = nlohmann::json::array({ "ok", 123, "also_ok" });
    HVStringList out;
    Check(!HVFieldJsonTraits<HVStringList>::Deserialize(bad_array, out),
          "StringList rejects non-string item");
}

static void TestGeometryListRoundTrip()
{
    HVGeometryList original;

    // Point
    HVGeometryInfo pt;
    pt.geometry_id_ = 1;
    pt.geometry_name_ = "pt";
    pt.SetPoint(HVPoint(10.0, 20.0));
    original.values.push_back(pt);

    // LineSegment
    HVGeometryInfo seg;
    seg.geometry_id_ = 2;
    seg.geometry_name_ = "seg";
    seg.SetLineSegment(HVLineSegment(HVPoint(0.0, 0.0), HVPoint(100.0, 100.0)));
    original.values.push_back(seg);

    // Rectangle
    HVGeometryInfo rect;
    rect.geometry_id_ = 3;
    rect.geometry_name_ = "rect";
    rect.SetRect(HVRect(5.0, 6.0, 50.0, 60.0));
    original.values.push_back(rect);

    // RotatedRectangle
    HVGeometryInfo rrect;
    rrect.geometry_id_ = 4;
    rrect.geometry_name_ = "rrect";
    rrect.SetRotatedRect(HVRotatedRect(HVPoint(30.0, 40.0), 70.0, 80.0, 45.0));
    original.values.push_back(rrect);

    // Box
    HVGeometryInfo box;
    box.geometry_id_ = 5;
    box.geometry_name_ = "box";
    box.SetBox(HVBox(HVPoint3D(1.0, 2.0, 3.0), 10.0, 20.0, 30.0));
    original.values.push_back(box);

    // RotatedBox
    HVGeometryInfo rbox;
    rbox.geometry_id_ = 6;
    rbox.geometry_name_ = "rbox";
    rbox.SetRotatedBox(HVRotatedBox(
        HVPoint3D(4.0, 5.0, 6.0), 40.0, 50.0, 60.0,
        HVOrientation3D(10.0, 20.0, 30.0)));
    original.values.push_back(rbox);

    nlohmann::json json_value;
    Check(HVFieldJsonTraits<HVGeometryList>::Serialize(original, json_value),
          "GeometryList serialize");
    Check(json_value.is_array(), "GeometryList json is array");
    Check(json_value.size() == 6, "GeometryList json size");

    HVGeometryList restored;
    Check(HVFieldJsonTraits<HVGeometryList>::Deserialize(json_value, restored),
          "GeometryList deserialize");
    Check(restored.values.size() == 6, "GeometryList restored size");

    Check(restored.values[0].geometry_id_ == 1, "Point id");
    Check(restored.values[0].shape_type_ == HVGeometryShapeType::Point, "Point shape_type");
    Check(restored.values[1].geometry_id_ == 2, "LineSegment id");
    Check(restored.values[2].geometry_id_ == 3, "Rectangle id");
    Check(restored.values[3].geometry_id_ == 4, "RotatedRectangle id");
    Check(restored.values[4].geometry_id_ == 5, "Box id");
    Check(restored.values[5].geometry_id_ == 6, "RotatedBox id");
}

static void TestGeometryListRejectsNonArray()
{
    HVGeometryList out;
    Check(!HVFieldJsonTraits<HVGeometryList>::Deserialize(nlohmann::json("bad"), out),
          "GeometryList rejects string");
    Check(!HVFieldJsonTraits<HVGeometryList>::Deserialize(nlohmann::json(99), out),
          "GeometryList rejects integer");
}

static void TestGeometryListRejectsUnknownShapeType()
{
    // shape_type = 99 does not exist in the enum
    nlohmann::json bad_item;
    bad_item["geometry_id"] = 1;
    bad_item["geometry_name"] = "bad";
    bad_item["shape_type"] = 99;
    bad_item["shape"] = nlohmann::json::object();

    nlohmann::json bad_array = nlohmann::json::array({ bad_item });
    HVGeometryList out;
    Check(!HVFieldJsonTraits<HVGeometryList>::Deserialize(bad_array, out),
          "GeometryList rejects unknown shape_type");

    // Also test: valid item then bad item (should fail the whole list)
    HVGeometryInfo pt;
    pt.SetPoint(HVPoint(0.0, 0.0));
    nlohmann::json valid_item;
    HVGeometryList single_list;
    single_list.values.push_back(pt);
    HVFieldJsonTraits<HVGeometryList>::Serialize(single_list, valid_item);
    // valid_item is now a single-element array; append the bad item
    valid_item.push_back(bad_item);

    HVGeometryList out2;
    Check(!HVFieldJsonTraits<HVGeometryList>::Deserialize(valid_item, out2),
          "GeometryList rejects mixed valid+bad items");
}

int main()
{
    std::printf("=== HVFieldJsonTraits tests ===\n");

    TestStringListRoundTrip();
    TestStringListRejectsNonArray();
    TestStringListRejectsNonStringItems();
    TestGeometryListRoundTrip();
    TestGeometryListRejectsNonArray();
    TestGeometryListRejectsUnknownShapeType();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
