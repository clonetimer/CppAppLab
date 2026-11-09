#include "static_file_handler.hpp"

#include <gtest/gtest.h>

TEST(StaticFileHandlerLearnerTest, MapsRootToIndexHtml) {
    FAIL() << "TODO(W8-STATIC-TEST-1): GET / should return www/index.html.";
}
TEST(StaticFileHandlerLearnerTest, ReturnsHtmlMimeType) {
    FAIL() << "TODO(W8-STATIC-TEST-2): verify text/html; charset=utf-8.";
}
TEST(StaticFileHandlerLearnerTest, Returns404ForMissingFile) {
    FAIL() << "TODO(W8-STATIC-TEST-3): missing static resource returns 404.";
}
TEST(StaticFileHandlerLearnerTest, Returns405ForNonGet) {
    FAIL() << "TODO(W8-STATIC-TEST-4): non-GET request returns 405 and Allow: GET.";
}
TEST(StaticFileHandlerLearnerTest, RejectsDotDotPathTraversal) {
    FAIL() << "TODO(W8-STATIC-TEST-5): reject a target containing ..";
}
TEST(StaticFileHandlerLearnerTest, RejectsBackslashTarget) {
    FAIL() << "TODO(W8-STATIC-TEST-6): reject backslash in target.";
}
TEST(StaticFileHandlerLearnerTest, StripsQueryString) {
    FAIL() << "TODO(W8-STATIC-TEST-7): ignore query string for file lookup.";
}
TEST(StaticFileHandlerLearnerTest, SelectsMimeTypeByExtension) {
    FAIL() << "TODO(W8-STATIC-TEST-8): select CSS, JS, TXT and JSON MIME types.";
}
TEST(StaticFileHandlerLearnerTest, ReadsFileInBinaryMode) {
    FAIL() << "TODO(W8-STATIC-TEST-9): preserve binary bytes without text conversion.";
}
TEST(StaticFileHandlerLearnerTest, SerializerComputesContentLength) {
    FAIL() << "TODO(W8-STATIC-TEST-10): serialized response length matches body bytes.";
}
