// 校验静态资源路径白名单、gzip 协商与 MIME 映射

#include <unity.h>

#include <deskquad/web/http_util.h>

using deskquad::AcceptsGzip;
using deskquad::IsSafeAssetPath;
using deskquad::MimeTypeForPath;

void setUp() {}
void tearDown() {}

static void test_normal_hashed_asset_is_allowed()
{
    TEST_ASSERT_TRUE(IsSafeAssetPath("/assets/app.a1b2c3d4.js"));
    TEST_ASSERT_TRUE(IsSafeAssetPath("/assets/app.a1b2c3d4.css"));
    TEST_ASSERT_TRUE(IsSafeAssetPath("/assets/app.a1b2c3d4.js.gz"));
}

static void test_asset_prefix_is_required()
{
    TEST_ASSERT_FALSE(IsSafeAssetPath("/index.html"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/ssid.json"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/ai_config.json"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/nested/"));
}

static void test_path_traversal_and_specials_are_rejected()
{
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/../ssid.json"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/..%2fssid.json"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets\\app.js"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/app.js?x=1"));
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/app js"));
}

static void test_long_path_is_rejected()
{
    TEST_ASSERT_FALSE(IsSafeAssetPath("/assets/app.0123456789abcdefg.js"));
    TEST_ASSERT_TRUE(IsSafeAssetPath("/assets/app.0123456789abcdef.js"));
}

static void test_accept_encoding_gzip()
{
    TEST_ASSERT_TRUE(AcceptsGzip("gzip"));
    TEST_ASSERT_TRUE(AcceptsGzip("deflate, gzip"));
    TEST_ASSERT_TRUE(AcceptsGzip("gzip;q=0.8"));
    TEST_ASSERT_TRUE(AcceptsGzip("br, *;q=0.5"));
    TEST_ASSERT_TRUE(AcceptsGzip("GZIP"));
}

static void test_accept_encoding_rejects_gzip()
{
    TEST_ASSERT_FALSE(AcceptsGzip(nullptr));
    TEST_ASSERT_FALSE(AcceptsGzip(""));
    TEST_ASSERT_FALSE(AcceptsGzip("identity"));
    TEST_ASSERT_FALSE(AcceptsGzip("gzip;q=0"));
    TEST_ASSERT_FALSE(AcceptsGzip("gzip ; q=0.0, deflate"));
}

static void test_mime_types()
{
    TEST_ASSERT_EQUAL_STRING("text/html; charset=utf-8", MimeTypeForPath("/index.html"));
    TEST_ASSERT_EQUAL_STRING("text/javascript; charset=utf-8", MimeTypeForPath("/assets/app.js"));
    TEST_ASSERT_EQUAL_STRING("text/css; charset=utf-8", MimeTypeForPath("/assets/app.css"));
    TEST_ASSERT_EQUAL_STRING("image/svg+xml", MimeTypeForPath("/assets/icon.svg"));
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", MimeTypeForPath("/assets/font.bin"));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_normal_hashed_asset_is_allowed);
    RUN_TEST(test_asset_prefix_is_required);
    RUN_TEST(test_path_traversal_and_specials_are_rejected);
    RUN_TEST(test_long_path_is_rejected);
    RUN_TEST(test_accept_encoding_gzip);
    RUN_TEST(test_accept_encoding_rejects_gzip);
    RUN_TEST(test_mime_types);
    return UNITY_END();
}
