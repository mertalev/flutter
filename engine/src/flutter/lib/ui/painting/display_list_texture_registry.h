// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_LIB_UI_PAINTING_DISPLAY_LIST_TEXTURE_REGISTRY_H_
#define FLUTTER_LIB_UI_PAINTING_DISPLAY_LIST_TEXTURE_REGISTRY_H_

#include <memory>

#include "flutter/display_list/image/dl_image.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/task_runner.h"
#include "flutter/lib/ui/snapshot_delegate.h"

namespace flutter {

/// A DlImage that resolves from a TextureRegistry texture on the raster thread.
///
/// On each raster-thread access to impeller_texture(), attempts to resolve
/// the texture via GetTextureImage (zero-copy) or Paint (fallback blit).
///
/// When freeze is true, caches after first successful resolution (snapshot).
/// When freeze is false, re-resolves every frame (live).
class DlImageTextureRegistry final : public DlImage {
 public:
  static sk_sp<DlImageTextureRegistry> Make(
      int64_t texture_id,
      const DlISize& size,
      bool freeze,
      fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
      fml::RefPtr<fml::TaskRunner> raster_task_runner);

  ~DlImageTextureRegistry() override = default;

  // |DlImage|
  sk_sp<SkImage> skia_image() const override;

  // |DlImage|
  std::shared_ptr<impeller::Texture> impeller_texture() const override;

  // |DlImage|
  bool isOpaque() const override { return false; }

  // |DlImage|
  bool isTextureBacked() const override;

  // |DlImage|
  bool isUIThreadSafe() const override { return true; }

  // |DlImage|
  bool Equals(const DlImage* other) const override {
    if (!freeze_ || !other) {
      return false;
    }
    auto cached = std::atomic_load(&cached_texture_);
    return cached && cached == other->impeller_texture();
  }

  // |DlImage|
  DlISize GetSize() const override { return size_; }

  // |DlImage|
  size_t GetApproximateByteSize() const override;

  // |DlImage|
  OwningContext owning_context() const override {
    return OwningContext::kRaster;
  }

  // |DlImage|
  std::optional<std::string> get_error() const override;

 private:
  DlImageTextureRegistry(
      int64_t texture_id,
      const DlISize& size,
      bool freeze,
      fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
      fml::RefPtr<fml::TaskRunner> raster_task_runner);

  int64_t texture_id_;
  DlISize size_;
  bool freeze_;
  fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate_;
  fml::RefPtr<fml::TaskRunner> raster_task_runner_;
  mutable std::shared_ptr<impeller::Texture> cached_texture_;
  mutable std::mutex error_mutex_;
  mutable std::optional<std::string> error_;

  FML_DISALLOW_COPY_AND_ASSIGN(DlImageTextureRegistry);
};

}  // namespace flutter

#endif  // FLUTTER_LIB_UI_PAINTING_DISPLAY_LIST_TEXTURE_REGISTRY_H_
