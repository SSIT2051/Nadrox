/*
 * Link-time-only stubs for missing androidfw C API symbols.
 * These provide placeholder definitions to satisfy unresolved references when
 * linking against the standalone ART stub tree; they are not runtime-correct
 * implementations.
 */

// Stub implementations for missing androidfw C API symbols
#include <stddef.h>

// Generic stubs returning 0/NULL
#define STUB_INT(name) int name(void) { return 0; }
#define STUB_PTR(name) void* name(void) { return 0; }

// We'll define each missing symbol as a function with a variadic signature to accept any args
// Use a macro that creates a function accepting arbitrary arguments via ...
#define STUB(name) int name(...) { return 0; }

STUB(ResolveAttrs)
STUB(ResXMLParser_indexOfStyle)
STUB(AssetManager_getLocales)
STUB(ResXMLParser_new)
STUB(RetrieveAttributes)
STUB(ApplyStyle)
STUB(ResXMLParser_getAttributeCount)
STUB(Asset_openFileDescriptor)
STUB(AssetManager_getResourceId)
STUB(Theme_delete)
STUB(ResXMLParser_getAttributeNameResID)
STUB(Theme_getAttribute)
STUB(ResXMLParser_restart)
STUB(AssetManager_unlock)
STUB(AssetDir_getFileCount)
STUB(AssetManager_setConfiguration)
STUB(AssetManager_resolveReference)
STUB(ApkAssets_loadDir)
STUB(ResStringPool_stringAt)
STUB(ResXMLTree_new)
STUB(AssetManager_newTheme)
STUB(ResXMLTree_setTo)
STUB(ResXMLParser_getElementNameID)
STUB(AssetManager_getResource)
STUB(Asset_delete)
STUB(Asset_seek)
STUB(AssetManager_getStringPoolForCookie)
STUB(ApkAssets_load)
STUB(AssetManager_getBag)
STUB(ResXMLParser_indexOfClass)
STUB(ResXMLParser_next)
STUB(ResXMLParser_getLineNumber)
STUB(AssetDir_getFileName)
STUB(ResXMLParser_getAttributeDataType)
STUB(Asset_getRemainingLength)
STUB(AssetManager_setApkAssets)
STUB(AssetManager_openNonAsset)
STUB(ResXMLParser_getStrings)
STUB(ResXMLParser_getAttributeValueStringID)
STUB(ResXMLParser_getAttributeNameID)
STUB(ResXMLParser_getAttributeValue)
STUB(AssetManager_getResourceName)
STUB(Asset_getBuffer)
STUB(ResXMLParser_delete)
STUB(Theme_applyStyle)
STUB(Asset_read)
STUB(ResXMLParser_getAttributeData)
STUB(AssetManager_openDir)
STUB(AssetManager_new)
STUB(AssetManager_lock)
STUB(ResXMLParser_indexOfAttribute)
STUB(Theme_setTo)
STUB(ResXMLTree_delete)
STUB(Asset_getLength)
STUB(AssetDir_getFileType)
STUB(AssetDir_delete)
