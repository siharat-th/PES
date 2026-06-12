// Minimal implementation of the Skia tools' Resources API that libpes links
// against (pesLoadOrDownloadAsset uses it to load stitch textures).
// The resource directory is set at runtime via pesffi::set_resource_path().

#include "include/core/SkData.h"
#include "include/core/SkString.h"

#include <string>

static std::string g_resourcePath = "resources";

void SetResourcePath(const char* path) {
    g_resourcePath = path ? path : "";
}

SkString GetResourcePath(const char* resource) {
    return SkString((g_resourcePath + "/" + (resource ? resource : "")).c_str());
}

sk_sp<SkData> GetResourceAsData(const char* resource) {
    return SkData::MakeFromFileName(GetResourcePath(resource).c_str());
}
