// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#import <Foundation/Foundation.h>

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/base_paths.h"
#include "base/data_pack.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "skia/ext/skia_utils_mac.h"

ResourceBundle::~ResourceBundle() {
  FreeImages();

  delete locale_resources_data_;
  locale_resources_data_ = NULL;
  delete resources_data_;
  resources_data_ = NULL;
  theme_data_ = NULL;
}

namespace {

base::DataPack *LoadResourceDataPack(NSString *name) {
  base::DataPack *resource_pack = NULL;

  NSString *resource_path = [mac_util::MainAppBundle() pathForResource:name
                                                                ofType:@"pak"];
  if (resource_path) {
    FilePath resources_pak_path([resource_path fileSystemRepresentation]);
    resource_pack = new base::DataPack;
    bool success = resource_pack->Load(resources_pak_path);
    if (!success) {
      delete resource_pack;
      resource_pack = NULL;
    }
  }

  return resource_pack;
}

}  // namespace

void ResourceBundle::LoadResources(const std::wstring& pref_locale) {
  DLOG_IF(WARNING, pref_locale.size() != 0)
      << "ignoring requested locale in favor of NSBundle's selection";

  DCHECK(resources_data_ == NULL) << "resource data already loaded!";
  resources_data_ = LoadResourceDataPack(@"chrome");
  DCHECK(resources_data_) << "failed to load chrome.pak";

  DCHECK(locale_resources_data_ == NULL) << "locale data already loaded!";
  locale_resources_data_ = LoadResourceDataPack(@"locale");
  DCHECK(locale_resources_data_) << "failed to load locale.pak";
}

void ResourceBundle::LoadThemeResources() {
  // The data has been merged with chrome.pak so just set the pointer to be
  // the same file.
  DCHECK(resources_data_);
  theme_data_ = resources_data_;
}

// static
RefCountedStaticMemory* ResourceBundle::LoadResourceBytes(
    DataHandle module, int resource_id) {
  DCHECK(module);
  return module->GetStaticMemory(resource_id);
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  DCHECK(resources_data_);
  base::StringPiece data;
  if (!resources_data_->GetStringPiece(resource_id, &data)) {
    if (!locale_resources_data_->GetStringPiece(resource_id, &data)) {
      return base::StringPiece();
    }
  }
  return data;
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  base::StringPiece data;
  if (!locale_resources_data_->GetStringPiece(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return string16();
    }
  }

  // Data pack encodes strings as UTF16.
  string16 msg(reinterpret_cast<const char16*>(data.data()),
               data.length() / 2);
  return msg;
}

NSImage* ResourceBundle::GetNSImageNamed(int resource_id) {
  // Currently this doesn't make a cache holding these as NSImages because
  // GetBitmapNamed has a cache, and we don't want to double cache.
  SkBitmap* bitmap = GetBitmapNamed(resource_id);
  if (!bitmap)
    return nil;

  NSImage* nsimage = gfx::SkBitmapToNSImage(*bitmap);
  return nsimage;
}
