// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/display_list_texture_registry.h"

#include <memory>
#include <mutex>

#include "flutter/display_list/dl_builder.h"
#include "flutter/fml/trace_event.h"
#include "impeller/core/texture.h"

namespace flutter {

sk_sp<DlImageTextureRegistry> DlImageTextureRegistry::Make(
    int64_t texture_id,
    const DlISize& size,
    bool freeze,
    fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
    fml::RefPtr<fml::TaskRunner> raster_task_runner) {
  return sk_sp<DlImageTextureRegistry>(new DlImageTextureRegistry(
      texture_id, size, freeze, std::move(snapshot_delegate),
      std::move(raster_task_runner)));
}

DlImageTextureRegistry::DlImageTextureRegistry(
    int64_t texture_id,
    const DlISize& size,
    bool freeze,
    fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
    fml::RefPtr<fml::TaskRunner> raster_task_runner)
    : texture_id_(texture_id),
      size_(size),
      freeze_(freeze),
      snapshot_delegate_(std::move(snapshot_delegate)),
      raster_task_runner_(std::move(raster_task_runner)) {}

sk_sp<SkImage> DlImageTextureRegistry::skia_image() const {
  return nullptr;
}

std::shared_ptr<impeller::Texture> DlImageTextureRegistry::impeller_texture()
    const {
  if (freeze_) {
    auto cached = std::atomic_load(&cached_texture_);
    if (cached) {
      return cached;
    }
  }

  // Resolution requires the raster thread
  if (!raster_task_runner_->RunsTasksOnCurrentThread()) {
    return nullptr;
  }

  TRACE_EVENT0("flutter", "DlImageTextureRegistry::impeller_texture (resolve)");

  auto snapshot_delegate = snapshot_delegate_.get();
  if (!snapshot_delegate) {
    std::scoped_lock lock(error_mutex_);
    error_ = "Snapshot delegate is missing.";
    return nullptr;
  }

  auto registry = snapshot_delegate->GetTextureRegistry();
  if (!registry) {
    std::scoped_lock lock(error_mutex_);
    error_ = "Texture registry is missing.";
    return nullptr;
  }

  auto texture = registry->GetTexture(texture_id_);
  if (!texture) {
    std::scoped_lock lock(error_mutex_);
    error_ = "Texture not found.";
    return nullptr;
  }

  DlRect bounds = DlRect::MakeWH(size_.width, size_.height);

  // Try zero-copy: ask the texture for its image directly.
  flutter::Texture::PaintContext ctx;
  auto aiks_context = snapshot_delegate->GetAiksContext();
  ctx.aiks_context = aiks_context.get();
  ctx.gr_context = snapshot_delegate->GetGrContext();
  sk_sp<DlImage> dl_image =
      texture->GetTextureImage(ctx, bounds, /*freeze=*/false);
  if (auto tex = dl_image ? dl_image->impeller_texture() : nullptr) {
    std::atomic_store(&cached_texture_, tex);
    return tex;
  }

  // Fallback: paint the texture into a DisplayList and rasterize.
  DisplayListBuilder builder(bounds);
  DlPaint paint;
  ctx.canvas = &builder;
  ctx.paint = &paint;
  texture->Paint(ctx, bounds, /*freeze=*/false, DlImageSampling::kLinear);
  auto display_list = builder.Build();

  if (display_list->op_count() == 0) {
    std::scoped_lock lock(error_mutex_);
    error_ = "Texture has no content.";
    return nullptr;
  }

  auto snapshot = snapshot_delegate->MakeRasterSnapshotSync(
      display_list, size_, SnapshotPixelFormat::kDontCare);
  if (auto tex = snapshot ? snapshot->impeller_texture() : nullptr) {
    std::atomic_store(&cached_texture_, tex);
    return tex;
  } else {
    std::scoped_lock lock(error_mutex_);
    error_ = "Failed to create snapshot.";
  }

  return std::atomic_load(&cached_texture_);
}

bool DlImageTextureRegistry::isTextureBacked() const {
  auto tex = std::atomic_load(&cached_texture_);
  return tex && tex->IsValid();
}

size_t DlImageTextureRegistry::GetApproximateByteSize() const {
  auto size = sizeof(DlImageTextureRegistry);
  auto tex = std::atomic_load(&cached_texture_);
  if (tex) {
    size += tex->GetTextureDescriptor().GetByteSizeOfBaseMipLevel();
  } else {
    size += size_.Area() * 4;
  }
  return size;
}

std::optional<std::string> DlImageTextureRegistry::get_error() const {
  std::scoped_lock lock(error_mutex_);
  return error_;
}

}  // namespace flutter
